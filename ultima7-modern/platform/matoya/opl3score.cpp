#include "opl3score.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;
constexpr size_t MaxFile = 16 * 1024 * 1024;
constexpr size_t MaxOutput = 32 * 1024 * 1024;
constexpr size_t MaxEvents = 200000;
constexpr uint32_t MaxTick = 0x0fffffff;

struct InvalidScore : std::runtime_error {
	InvalidScore() : std::runtime_error("unsupported score") {}
};

void require(bool condition)
{
	if (!condition)
		throw InvalidScore();
}

void range(const Bytes &data, size_t pos, size_t count)
{
	require(pos <= data.size() && count <= data.size() - pos);
}

uint16_t le16(const Bytes &data, size_t pos)
{
	range(data, pos, 2);
	return uint16_t(data[pos] | data[pos + 1] << 8);
}

uint32_t le32(const Bytes &data, size_t pos)
{
	range(data, pos, 4);
	return uint32_t(data[pos]) | uint32_t(data[pos + 1]) << 8 |
		uint32_t(data[pos + 2]) << 16 | uint32_t(data[pos + 3]) << 24;
}

uint32_t be32(const Bytes &data, size_t pos)
{
	range(data, pos, 4);
	return uint32_t(data[pos]) << 24 | uint32_t(data[pos + 1]) << 16 |
		uint32_t(data[pos + 2]) << 8 | uint32_t(data[pos + 3]);
}

void put32(Bytes &data, size_t pos, uint32_t value, bool little)
{
	range(data, pos, 4);
	for (size_t i = 0; i < 4; ++i)
		data[pos + i] = uint8_t(value >> ((little ? i : 3 - i) * 8));
}

Bytes slice(const Bytes &data, size_t pos, size_t count)
{
	range(data, pos, count);
	return Bytes(data.begin() + pos, data.begin() + pos + count);
}

void append(Bytes &out, const Bytes &data)
{
	require(out.size() <= MaxOutput && data.size() <= MaxOutput - out.size());
	out.insert(out.end(), data.begin(), data.end());
}

void byte(Bytes &out, uint8_t value)
{
	require(out.size() < MaxOutput);
	out.push_back(value);
}

uint32_t readVlq(const Bytes &data, size_t &pos)
{
	uint32_t value = 0;
	for (int i = 0; i < 4; ++i) {
		range(data, pos, 1);
		uint8_t b = data[pos++];
		value = value << 7 | (b & 127);
		if (b < 128)
			return value;
	}
	throw InvalidScore();
}

void writeVlq(Bytes &out, uint32_t value)
{
	require(value <= MaxTick);
	uint8_t encoded[4];
	int count = 0;
	encoded[count++] = uint8_t(value & 127);
	while ((value >>= 7) != 0)
		encoded[count++] = uint8_t((value & 127) | 128);
	while (count)
		byte(out, encoded[--count]);
}

uint32_t addTick(uint32_t tick, uint32_t delta)
{
	require(delta <= MaxTick - tick);
	return tick + delta;
}

bool readFile(const char *path, Bytes &out)
{
	if (!path || !*path)
		return false;
	std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path, "rb"), &std::fclose);
	if (!file)
		return false;
	bool okay = std::fseek(file.get(), 0, SEEK_END) == 0;
	long size = okay ? std::ftell(file.get()) : -1;
	okay = size >= 0 && size <= long(MaxFile) && std::fseek(file.get(), 0, SEEK_SET) == 0;
	if (okay) {
		out.resize(size_t(size));
		okay = std::fread(out.data(), 1, out.size(), file.get()) == out.size();
	}
	return okay;
}

struct FlexEntry {
	uint32_t offset, size;
};

std::vector<FlexEntry> flexEntries(const Bytes &data)
{
	require(le32(data, 80) == 0xffff1a00);
	uint32_t count = le32(data, 84);
	require(count <= 4096);
	size_t header = 128 + size_t(count) * 8;
	range(data, 0, header);
	std::vector<FlexEntry> entries;
	for (uint32_t i = 0; i < count; ++i) {
		FlexEntry entry{le32(data, 128 + i * 8), le32(data, 132 + i * 8)};
		if (entry.size) {
			require(entry.offset >= header);
			range(data, entry.offset, entry.size);
		}
		entries.push_back(entry);
	}
	return entries;
}

Bytes rebuildFlex(const Bytes &original, const std::vector<FlexEntry> &entries,
	const std::map<size_t, Bytes> &replacements)
{
	Bytes out = slice(original, 0, 128 + entries.size() * 8);
	for (size_t i = 0; i < entries.size(); ++i) {
		if (!entries[i].size) {
			put32(out, 128 + i * 8, 0, true);
			put32(out, 132 + i * 8, 0, true);
			continue;
		}
		auto replacement = replacements.find(i);
		Bytes content = replacement == replacements.end() ?
			slice(original, entries[i].offset, entries[i].size) : replacement->second;
		put32(out, 128 + i * 8, uint32_t(out.size()), true);
		put32(out, 132 + i * 8, uint32_t(content.size()), true);
		append(out, content);
	}
	return out;
}

struct Event {
	uint32_t tick = 0;
	uint8_t status = 0, first = 0, second = 0;
	uint32_t duration = 0;
	Bytes encoded;
	bool noteOn() const { return (status & 240) == 144 && second != 0; }
	bool noteOff() const { return (status & 240) == 128 || ((status & 240) == 144 && second == 0); }
	bool control(uint8_t number) const { return (status & 240) == 176 && first == number; }
	int channel() const { return status & 15; }
	bool end() const { return status == 255 && first == 47; }
};

struct Track {
	Bytes original;
	std::vector<Event> events;
	bool changed = false, exactTiming = true;
};

Track parseTrack(const Bytes &data, bool xmi)
{
	Track track;
	track.original = data;
	size_t pos = 0;
	uint32_t tick = 0;
	uint8_t running = 0;
	bool ended = false;
	while (pos < data.size()) {
		require(track.events.size() < MaxEvents);
		if (xmi) {
			while (pos < data.size() && data[pos] < 128) {
				// A zero interval still consumes a native timer tick.
				track.exactTiming &= data[pos] != 0;
				tick = addTick(tick, data[pos++]);
			}
		} else
			tick = addTick(tick, readVlq(data, pos));
		range(data, pos, 1);
		size_t start = pos;
		Event event;
		event.tick = tick;
		event.status = data[pos];
		bool explicitStatus = event.status >= 128;
		if (explicitStatus) {
			++pos;
			if (event.status < 240)
				running = event.status;
		} else {
			require(!xmi && running != 0);
			event.status = running;
		}
		uint8_t kind = event.status & 240;
		if (event.status == 255) {
			range(data, pos, 1);
			event.first = data[pos++];
			uint32_t length = readVlq(data, pos);
			range(data, pos, length);
			pos += length;
			if (event.first == 47) {
				require(length == 0);
				ended = true;
			}
		} else if (event.status == 240 || event.status == 247) {
			uint32_t length = readVlq(data, pos);
			range(data, pos, length);
			pos += length;
		} else if (!xmi && event.status == 254) {
			range(data, pos, 2);
			event.first = data[pos];
			uint8_t length = data[pos + 1];
			pos += 2;
			range(data, pos, length);
			pos += length;
		} else {
			require(event.status >= 128 && event.status < 240 && (!xmi || kind != 128));
			size_t count = kind == 192 || kind == 208 ? 1 : 2;
			range(data, pos, count);
			event.first = data[pos++];
			if (count == 2)
				event.second = data[pos++];
			require(event.first < 128 && event.second < 128);
			if (xmi && kind == 144)
				event.duration = readVlq(data, pos);
		}
		event.encoded = slice(data, start, pos - start);
		if (!explicitStatus)
			event.encoded.insert(event.encoded.begin(), event.status);
		track.events.push_back(std::move(event));
		if (ended)
			break;
	}
	require(ended);
	while (pos < data.size())
		require(data[pos++] == 0);
	return track;
}

Bytes serializeTrack(const Track &track, bool xmi)
{
	if (!track.changed)
		return track.original;
	Bytes out;
	uint32_t tick = 0;
	for (const Event &event : track.events) {
		require(event.tick >= tick);
		uint32_t delta = event.tick - tick;
		if (xmi) {
			while (delta >= 127) {
				byte(out, 127);
				delta -= 127;
			}
			if (delta)
				byte(out, uint8_t(delta));
		} else
			writeVlq(out, delta);
		append(out, event.encoded);
		tick = event.tick;
	}
	return out;
}

struct Patch {
	int program = -1, bank = 0;
	bool operator==(const Patch &) const = default;
};

struct Note {
	uint32_t start, duration;
	int channel, pitch, velocity;
	size_t onTrack, onEvent, offTrack = 0, offEvent = 0;
	Patch patch;
};

using NoteKey = std::tuple<int, uint32_t, uint32_t, int, int>;
NoteKey noteKey(const Note &note)
{
	return {note.channel, note.start, note.duration, note.pitch, note.velocity};
}

struct Score {
	bool xmi = false, unsafe = false, branchOffsets = false;
	uint16_t ppqn = 0;
	std::vector<Track> tracks;
	std::vector<Note> notes;
	std::set<std::pair<int, int>> timbres;
};

struct EventRef {
	size_t track, index;
};

std::vector<EventRef> eventOrder(const Score &score)
{
	std::vector<EventRef> order;
	for (size_t t = 0; t < score.tracks.size(); ++t)
		for (size_t i = 0; i < score.tracks[t].events.size(); ++i)
			order.push_back({t, i});
	require(order.size() <= MaxEvents);
	std::sort(order.begin(), order.end(), [&](EventRef a, EventRef b) {
		return std::tuple(score.tracks[a.track].events[a.index].tick, a.track, a.index) <
			std::tuple(score.tracks[b.track].events[b.index].tick, b.track, b.index);
	});
	return order;
}

void collectNotes(Score &score)
{
	for (const Track &track : score.tracks)
		if (!track.exactTiming)
			score.unsafe = true;
	std::array<Patch, 16> patches;
	std::map<std::pair<int, int>, std::deque<size_t>> pending;
	for (EventRef ref : eventOrder(score)) {
		const Event &event = score.tracks[ref.track].events[ref.index];
		int channel = event.channel();
		if (event.status == 254 || event.control(115))
			score.unsafe = true;
		if ((event.status & 240) == 192)
			patches[channel].program = event.first;
		else if (event.control(114))
			patches[channel].bank = event.second;
		else if (event.noteOn()) {
			Note note{event.tick, event.duration, channel, event.first, event.second,
				ref.track, ref.index, 0, 0, patches[channel]};
			if (score.xmi)
				addTick(note.start, note.duration);
			else
				pending[{channel, event.first}].push_back(score.notes.size());
			score.notes.push_back(note);
		} else if (!score.xmi && event.noteOff()) {
			auto &queue = pending[{channel, event.first}];
			if (queue.empty())
				score.unsafe = true;
			else {
				Note &note = score.notes[queue.front()];
				queue.pop_front();
				note.duration = event.tick - note.start;
				note.offTrack = ref.track;
				note.offEvent = ref.index;
			}
		}
	}
	for (const auto &[key, queue] : pending)
		if (!queue.empty())
			score.unsafe = true;
}

struct Chunk {
	std::array<uint8_t, 4> tag{};
	Bytes payload;
	std::array<uint8_t, 4> type{};
	std::vector<Chunk> children;
	int score = -1, track = -1;
	bool container = false;
	uint8_t padding = 0;
	bool is(const char *id) const { return std::equal(tag.begin(), tag.end(), id); }
};

struct Document {
	bool xmi = false;
	std::vector<Score> scores;
	std::vector<Chunk> chunks;
};

std::vector<Chunk> parseIff(const Bytes &data, size_t start, size_t end,
	Document &document, int depth)
{
	require(depth <= 8);
	std::vector<Chunk> chunks;
	while (start < end) {
		require(end - start >= 8);
		range(data, start, 8);
		uint32_t size = be32(data, start + 4);
		require(size <= end - start - 8);
		size_t stop = start + 8 + size;
		Chunk chunk;
		std::copy_n(data.begin() + start, 4, chunk.tag.begin());
		chunk.container = chunk.is("FORM") || chunk.is("CAT ") || chunk.is("LIST");
		if (chunk.container) {
			require(size >= 4);
			std::copy_n(data.begin() + start + 8, 4, chunk.type.begin());
			chunk.children = parseIff(data, start + 12, stop, document, depth + 1);
			if (chunk.is("FORM") && std::equal(chunk.type.begin(), chunk.type.end(), "XMID")) {
				Score score;
				score.xmi = true;
				int streamCount = 0;
				for (Chunk &child : chunk.children) {
					if (child.is("EVNT")) {
						++streamCount;
						child.score = int(document.scores.size());
						score.tracks.push_back(parseTrack(child.payload, true));
					} else if (child.is("TIMB")) {
						uint16_t count = le16(child.payload, 0);
						require(child.payload.size() == 2 + size_t(count) * 2);
						for (size_t i = 0; i < count; ++i)
							score.timbres.emplace(child.payload[3 + i * 2], child.payload[2 + i * 2]);
					} else if (child.is("RBRN")) {
						score.unsafe = true;
						score.branchOffsets = true;
					}
				}
				require(streamCount == 1);
				collectNotes(score);
				document.scores.push_back(std::move(score));
			}
		} else
			chunk.payload = slice(data, start + 8, size);
		start = stop;
		if (size & 1) {
			require(start < end);
			chunk.padding = data[start++];
		}
		chunks.push_back(std::move(chunk));
		require(chunks.size() <= 4096);
	}
	require(start == end);
	return chunks;
}

Document parseDocument(const Bytes &data)
{
	Document document;
	range(data, 0, 4);
	if (std::equal(data.begin(), data.begin() + 4, "MThd")) {
		require(be32(data, 4) == 6);
		range(data, 8, 6);
		uint16_t format = uint16_t(data[8] << 8 | data[9]);
		uint16_t count = uint16_t(data[10] << 8 | data[11]);
		require(format <= 1 && count > 0 && count <= 32);
		Score score;
		score.ppqn = uint16_t(data[12] << 8 | data[13]);
		require(score.ppqn > 0 && score.ppqn < 32768);
		size_t pos = 0;
		while (pos < data.size()) {
			range(data, pos, 8);
			uint32_t size = be32(data, pos + 4);
			Chunk chunk;
			std::copy_n(data.begin() + pos, 4, chunk.tag.begin());
			chunk.payload = slice(data, pos + 8, size);
			if (chunk.is("MTrk")) {
				chunk.track = int(score.tracks.size());
				score.tracks.push_back(parseTrack(chunk.payload, false));
			}
			document.chunks.push_back(std::move(chunk));
			pos += 8 + size_t(size);
			require(document.chunks.size() <= 4096);
		}
		require(score.tracks.size() == count);
		collectNotes(score);
		document.scores.push_back(std::move(score));
	} else {
		document.xmi = true;
		document.chunks = parseIff(data, 0, data.size(), document, 0);
		require(!document.scores.empty() && document.scores.size() <= 256);
	}
	return document;
}

Bytes serializeChunks(const std::vector<Chunk> &chunks, const Document &document)
{
	Bytes out;
	for (const Chunk &chunk : chunks) {
		Bytes payload;
		if (chunk.container) {
			payload.assign(chunk.type.begin(), chunk.type.end());
			append(payload, serializeChunks(chunk.children, document));
		} else if (chunk.score >= 0)
			payload = serializeTrack(document.scores[size_t(chunk.score)].tracks[0], true);
		else if (chunk.track >= 0)
			payload = serializeTrack(document.scores[0].tracks[size_t(chunk.track)], false);
		else
			payload = chunk.payload;
		for (uint8_t b : chunk.tag)
			byte(out, b);
		size_t lengthPos = out.size();
		append(out, Bytes(4));
		put32(out, lengthPos, uint32_t(payload.size()), false);
		append(out, payload);
		if (document.xmi && (payload.size() & 1))
			byte(out, chunk.padding);
	}
	return out;
}

Event controller(int channel, uint32_t tick, uint8_t number, uint8_t value)
{
	Event event;
	event.tick = tick;
	event.status = uint8_t(176 | channel);
	event.first = number;
	event.second = value;
	event.encoded = {event.status, number, value};
	return event;
}

void insertBeforeNotes(Track &track, Event event)
{
	auto pos = std::find_if(track.events.begin(), track.events.end(), [&](const Event &existing) {
		return existing.tick > event.tick || (existing.tick == event.tick &&
			(existing.noteOn() || existing.noteOff() || existing.end()));
	});
	require(pos != track.events.end());
	track.events.insert(pos, std::move(event));
	track.changed = true;
}

std::map<NoteKey, size_t> noteCounts(const Score &score, int channel = -1)
{
	std::map<NoteKey, size_t> counts;
	for (const Note &note : score.notes)
		if (channel < 0 || note.channel == channel)
			++counts[noteKey(note)];
	return counts;
}

using ControlKey = std::tuple<uint32_t, int, int, int>;
std::vector<ControlKey> loopControls(const Score &score)
{
	std::vector<ControlKey> keys;
	for (EventRef ref : eventOrder(score)) {
		const Event &event = score.tracks[ref.track].events[ref.index];
		if (event.control(81) || event.control(82) || event.control(116) || event.control(117))
			keys.emplace_back(event.tick, event.channel(), event.first, event.second);
	}
	return keys;
}

std::vector<std::pair<uint32_t, Bytes>> tempos(const Score &score)
{
	std::vector<std::pair<uint32_t, Bytes>> keys;
	for (EventRef ref : eventOrder(score)) {
		const Event &event = score.tracks[ref.track].events[ref.index];
		if (event.status == 255 && event.first == 81)
			keys.emplace_back(event.tick, event.encoded);
	}
	return keys;
}

std::vector<ControlKey> bendsAndSustain(const Score &score, int channel)
{
	std::vector<ControlKey> keys;
	// Compare changes from MIDI's neutral bend and released pedal.
	int bend = 8192, sustain = 0;
	for (EventRef ref : eventOrder(score)) {
		const Event &event = score.tracks[ref.track].events[ref.index];
		if (event.channel() != channel)
			continue;
		bool changed = false;
		if (event.control(64)) {
			changed = sustain != event.second;
			sustain = event.second;
		} else if ((event.status & 240) == 224) {
			int value = event.first | event.second << 7;
			changed = bend != value;
			bend = value;
		}
		if (changed)
			keys.emplace_back(event.tick, event.status, event.first, event.second);
	}
	return keys;
}

int noteTrack(const Score &score, int channel)
{
	int track = -1;
	for (const Note &note : score.notes) {
		if (note.channel != channel)
			continue;
		if (track >= 0 && track != int(note.onTrack))
			return -1;
		track = int(note.onTrack);
	}
	return track;
}

Patch constantPatch(const Score &score, int channel)
{
	Patch patch;
	for (const Note &note : score.notes) {
		if (note.channel != channel)
			continue;
		if (note.patch.program < 0)
			return {};
		if (patch.program >= 0 && patch != note.patch)
			return {};
		patch = note.patch;
	}
	for (const Track &track : score.tracks)
		for (const Event &event : track.events)
			if (event.channel() == channel &&
				(((event.status & 240) == 192 && event.first != patch.program) ||
				(event.control(114) && event.second != patch.bank)))
				return {};
	return patch;
}

std::filesystem::path timbrePath(int game, const std::filesystem::path &directory)
{
	std::string wanted = game == 0 ? "u7voice.flx" : "xmidi.ad";
	std::error_code error;
	std::filesystem::directory_iterator entry(directory, error), end;
	for (; !error && entry != end; entry.increment(error)) {
		std::string name = entry->path().filename().string();
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
		if (name == wanted)
			return entry->path();
	}
	return {};
}

class TimbreCoverage {
	int game;
	std::filesystem::path directory;
	bool loaded = false;
	std::set<std::pair<int, int>> patches;

	void load()
	{
		loaded = true;
		std::filesystem::path path = timbrePath(game, directory);
		Bytes data;
		if (path.empty() || !readFile(path.string().c_str(), data))
			return;
		if (game == 0) {
			auto entries = flexEntries(data);
			require(entries.size() > 1 && entries[1].size > 0);
			Bytes bank = slice(data, entries[1].offset, entries[1].size);
			uint8_t count = bank[0];
			range(bank, 1, size_t(count) * 48);
			std::set<int> seen;
			for (size_t i = 0; i < count; ++i) {
				size_t pos = 1 + i * 48;
				int program = bank[pos + 47];
				if (!seen.insert(program).second)
					continue;
				if (bank[pos + 38] == 0 && bank[pos + 11] == 0)
					patches.emplace(0, program);
			}
		} else {
			bool ended = false;
			std::set<std::pair<int, int>> seen;
			for (size_t pos = 0; pos < data.size(); pos += 6) {
				range(data, pos, 6);
				int patch = data[pos], bank = data[pos + 1];
				if (bank == 255) {
					ended = true;
					break;
				}
				uint32_t offset = le32(data, pos + 2);
				uint16_t size = le16(data, offset);
				require(size >= 2);
				range(data, offset, size);
				if (seen.emplace(bank, patch).second && patch < 128 && bank < 128 && size == 14)
					patches.emplace(bank, patch);
			}
			require(ended);
		}
	}

public:
	TimbreCoverage(int playedGame, const char *fmPath) : game(playedGame),
		directory(std::filesystem::path(fmPath).parent_path()) {}

	bool has(Patch patch)
	{
		if (!loaded) {
			try { load(); }
			catch (...) { patches.clear(); }
		}
		return patches.contains({patch.bank, patch.program});
	}
};

/* Pan affects new notes, so its place among same-tick notes matters. */
struct PanPoint {
	uint32_t tick;
	uint8_t value;
	size_t notesBefore;
	bool operator==(const PanPoint &) const = default;
};

std::vector<PanPoint> panPoints(const Score &score, int channel)
{
	std::vector<PanPoint> points;
	uint32_t tick = 0;
	size_t notes = 0;
	for (auto ref : eventOrder(score)) {
		const Event &event = score.tracks[ref.track].events[ref.index];
		if (event.tick != tick) {
			tick = event.tick;
			notes = 0;
		}
		if (event.channel() == channel) {
			if (event.noteOn())
				++notes;
			else if (event.control(10))
				points.push_back({event.tick, event.second, notes});
		}
	}
	return points;
}

std::map<uint32_t, std::vector<NoteKey>> noteOrder(const Score &score, int channel)
{
	std::vector<const Note *> notes;
	for (const Note &note : score.notes)
		if (note.channel == channel)
			notes.push_back(&note);
	std::sort(notes.begin(), notes.end(), [](const Note *a, const Note *b) {
		return std::tie(a->start, a->onTrack, a->onEvent) < std::tie(b->start, b->onTrack, b->onEvent);
	});
	std::map<uint32_t, std::vector<NoteKey>> order;
	for (const Note *note : notes)
		order[note->start].push_back(noteKey(*note));
	return order;
}

void insertPan(Track &track, int channel, const PanPoint &point)
{
	auto at = track.events.begin();
	size_t notes = 0;
	while (at != track.events.end() && at->tick < point.tick)
		++at;
	while (at != track.events.end() && at->tick == point.tick && !at->end()) {
		if (at->channel() == channel && at->noteOn()) {
			if (notes == point.notesBefore)
				break;
			++notes;
		}
		++at;
	}
	track.events.insert(at, controller(channel, point.tick, 10, point.value));
	track.changed = true;
}

bool overlayMusic(Score &fm, const Score &mt, TimbreCoverage &coverage)
{
	if (fm.unsafe || mt.unsafe || fm.xmi != mt.xmi || fm.ppqn != mt.ppqn ||
		tempos(fm) != tempos(mt) || loopControls(fm) != loopControls(mt))
		return false;
	bool changed = false;
	for (int channel = 0; channel < 16; ++channel) {
		if (channel == 9 || noteCounts(fm, channel).empty() ||
			noteCounts(fm, channel) != noteCounts(mt, channel) ||
			bendsAndSustain(fm, channel) != bendsAndSustain(mt, channel))
			continue;
		int target = noteTrack(fm, channel);
		if (target < 0)
			continue;
		auto original = panPoints(fm, channel), desired = panPoints(mt, channel);
		if (desired.empty() || original == desired)
			continue;
		auto fmOrder = noteOrder(fm, channel), mtOrder = noteOrder(mt, channel);
		bool aligned = true;
		for (const PanPoint &point : desired)
			if (point.notesBefore > 0 && point.notesBefore < mtOrder[point.tick].size() &&
				fmOrder[point.tick] != mtOrder[point.tick])
				aligned = false;
		uint32_t end = fm.tracks[size_t(target)].events.back().tick;
		if (!aligned || desired.back().tick > end)
			continue;
		for (Track &track : fm.tracks) {
			auto removed = std::erase_if(track.events, [&](const Event &event) {
				return event.channel() == channel && event.control(10);
			});
			track.changed |= removed != 0;
		}
		for (const PanPoint &point : desired)
			insertPan(fm.tracks[size_t(target)], channel, point);
		changed = true;
	}

	// Looped arrangements need a separate correspondence check before adding notes.
	if (!loopControls(fm).empty())
		return changed;
	auto remaining = noteCounts(mt);
	for (const auto &[key, count] : noteCounts(fm)) {
		if (remaining[key] < count)
			return changed;
		remaining[key] -= count;
	}
	std::map<int, int> eligible;
	for (const Note &note : mt.notes) {
		if (!remaining[noteKey(note)] || note.channel == 9 || eligible.contains(note.channel))
			continue;
		int target = noteTrack(fm, note.channel);
		Patch patch = constantPatch(fm, note.channel);
		if (target < 0 || patch.program < 0 || patch != constantPatch(mt, note.channel) ||
			bendsAndSustain(fm, note.channel) != bendsAndSustain(mt, note.channel) ||
			(fm.xmi && !fm.timbres.contains({patch.bank, patch.program})) || !coverage.has(patch))
			continue;
		uint32_t first = MaxTick, earliest = MaxTick, latest = 0;
		for (const Note &original : fm.notes)
			if (original.channel == note.channel)
				first = std::min(first, original.start);
		for (const Note &extra : mt.notes)
			if (extra.channel == note.channel && remaining[noteKey(extra)]) {
				earliest = std::min(earliest, extra.start);
				latest = std::max(latest, addTick(extra.start, extra.duration));
			}
		if (earliest < first)
			continue;
		if (fm.tracks[size_t(target)].events.back().tick < latest) {
			// A later existing track can carry the same channel without moving end markers.
			int later = -1;
			for (size_t t = size_t(target); t < fm.tracks.size(); ++t)
				if (fm.tracks[t].events.back().tick >= latest) {
					later = int(t);
					break;
				}
			if (later < 0)
				continue;
			target = later;
		}
		eligible.emplace(note.channel, target);
	}
	for (const Note &note : mt.notes) {
		auto &count = remaining[noteKey(note)];
		if (!count || !eligible.contains(note.channel))
			continue;
		--count;
		int target = eligible.at(note.channel);
		insertBeforeNotes(fm.tracks[size_t(target)], mt.tracks[note.onTrack].events[note.onEvent]);
		if (!fm.xmi)
			insertBeforeNotes(fm.tracks[size_t(target)], mt.tracks[note.offTrack].events[note.offEvent]);
		changed = true;
	}
	return changed;
}

bool injectEffectPan(Score &score)
{
	if (!score.xmi || score.tracks.size() != 1 || score.branchOffsets)
		return false;
	Track &track = score.tracks[0];
	if (!track.exactTiming)
		return false;
	std::array<int, 16> indirect;
	indirect.fill(-1);
	std::array<size_t, 16> prefixPosition{};
	std::array<bool, 16> runtimePan{}, used{};
	std::map<size_t, std::vector<Event>> injections;
	for (size_t i = 0; i < track.events.size(); ++i) {
		const Event &event = track.events[i];
		int channel = event.channel();
		if (event.status == 254)
			return false;
		if ((event.status & 240) == 176) {
			int pending = indirect[channel];
			indirect[channel] = -1;
			if (event.control(115)) {
				if (pending >= 0)
					return false;
				indirect[channel] = event.second;
				prefixPosition[channel] = i;
			} else if (event.control(10))
				runtimePan[channel] = pending == 0;
			else if (event.control(110) || event.control(121))
				runtimePan[channel] = false;
		} else if (event.noteOn() && !used[channel]) {
			used[channel] = true;
			if (runtimePan[channel])
				continue;
			// Insert ahead of a pending prefix so its next controller keeps its value.
			size_t position = indirect[channel] >= 0 ? prefixPosition[channel] : i;
			uint32_t tick = track.events[position].tick;
			auto &events = injections[position];
			events.push_back(controller(channel, tick, 115, 0));
			events.push_back(controller(channel, tick, 10, 64));
		}
	}
	if (injections.empty())
		return false;
	std::vector<Event> events;
	for (size_t i = 0; i < track.events.size(); ++i) {
		auto insertion = injections.find(i);
		if (insertion != injections.end())
			events.insert(events.end(), insertion->second.begin(), insertion->second.end());
		events.push_back(track.events[i]);
	}
	track.events = std::move(events);
	track.changed = true;
	return true;
}

bool pairDocuments(Document &fm, const Document &mt, TimbreCoverage &coverage)
{
	if (fm.scores.size() != mt.scores.size())
		return false;
	bool changed = false;
	for (size_t i = 0; i < fm.scores.size(); ++i)
		changed |= overlayMusic(fm.scores[i], mt.scores[i], coverage);
	return changed;
}

Bytes transform(int game, const Bytes &fm, const Bytes &mt, int kind, TimbreCoverage &coverage)
{
	if (kind == U7_OPL3_SCORE_MUSIC_BANK || kind == U7_OPL3_SCORE_BG_INTRO_BANK ||
		kind == U7_OPL3_SCORE_SI_MENU || kind == U7_OPL3_SCORE_SI_EFFECTS) {
		auto entries = flexEntries(fm);
		std::vector<FlexEntry> partners;
		if (kind == U7_OPL3_SCORE_MUSIC_BANK || kind == U7_OPL3_SCORE_BG_INTRO_BANK)
			partners = flexEntries(mt);
		std::map<size_t, Bytes> replacements;
		for (size_t i = 0; i < entries.size(); ++i) {
			if (!entries[i].size)
				continue;
			if (kind == U7_OPL3_SCORE_SI_MENU && i != 27)
				continue;
			try {
				Document original = parseDocument(slice(fm, entries[i].offset, entries[i].size));
				bool expectedXmi = kind == U7_OPL3_SCORE_SI_EFFECTS ||
					(kind == U7_OPL3_SCORE_MUSIC_BANK && game == 1);
				if (original.xmi != expectedXmi)
					continue;
				bool changed = false;
				if (kind == U7_OPL3_SCORE_SI_EFFECTS) {
					for (Score &score : original.scores)
						changed |= injectEffectPan(score);
				} else {
					size_t partner = kind == U7_OPL3_SCORE_SI_MENU ? 28 : i;
					const Bytes &bank = kind == U7_OPL3_SCORE_SI_MENU ? fm : mt;
					const auto &table = kind == U7_OPL3_SCORE_SI_MENU ? entries : partners;
					if (partner >= table.size() || !table[partner].size)
						continue;
					Document other = parseDocument(slice(bank, table[partner].offset, table[partner].size));
					changed = pairDocuments(original, other, coverage);
				}
				if (changed) {
					Bytes replacement = serializeChunks(original.chunks, original);
					if (replacement != slice(fm, entries[i].offset, entries[i].size))
						replacements.emplace(i, std::move(replacement));
				}
			} catch (const InvalidScore &) {
				// A rejected entry remains byte-for-byte original in the rebuilt bank.
			}
		}
		return replacements.empty() ? Bytes{} : rebuildFlex(fm, entries, replacements);
	}
	Document original = parseDocument(fm);
	bool changed = false;
	if (kind == U7_OPL3_SCORE_BG_ENDING) {
		require(game == 0 && original.xmi && original.scores.size() == 5);
		changed |= overlayMusic(original.scores[3], original.scores[1], coverage);
		changed |= overlayMusic(original.scores[4], original.scores[2], coverage);
	} else if (kind == U7_OPL3_SCORE_XMIDI_PAIR) {
		require(game == 1 && original.xmi && !mt.empty());
		Document other = parseDocument(mt);
		require(other.xmi);
		changed = pairDocuments(original, other, coverage);
	}
	if (!changed)
		return {};
	Bytes out = serializeChunks(original.chunks, original);
	return out == fm ? Bytes{} : out;
}

struct FileVersion {
	std::filesystem::path path;
	uintmax_t size = 0;
	std::filesystem::file_time_type modified{};
	bool exists = false;
	bool operator==(const FileVersion &) const = default;
};

FileVersion fileVersion(const std::filesystem::path &path)
{
	FileVersion version{path};
	if (path.empty())
		return version;
	std::error_code error;
	uintmax_t size = std::filesystem::file_size(path, error);
	if (error)
		return version;
	auto modified = std::filesystem::last_write_time(path, error);
	if (!error) {
		version.size = size;
		version.modified = modified;
		version.exists = true;
	}
	return version;
}

struct ScoreSources {
	FileVersion fm, mt, timbre;
	bool operator==(const ScoreSources &) const = default;
};

ScoreSources scoreSources(int game, const std::filesystem::path &fm,
	const std::filesystem::path &mt, int kind)
{
	return {fileVersion(fm), fileVersion(mt), fileVersion(kind == U7_OPL3_SCORE_SI_EFFECTS ?
		std::filesystem::path{} : timbrePath(game, fm.parent_path()))};
}

struct CachedScore {
	int game, kind;
	ScoreSources sources;
	Bytes output;
};

constexpr size_t MaxCachedScores = 16;
constexpr size_t MaxCacheBytes = 64 * 1024 * 1024;

struct ScoreCache {
	std::mutex mutex;
	std::list<CachedScore> entries;
	size_t bytes = 0;
};

ScoreCache &scoreCache()
{
	static ScoreCache cache;
	return cache;
}

FILE *scoreStream(const Bytes &data)
{
	if (data.empty())
		return nullptr;
	FILE *file = std::tmpfile();
	if (!file)
		return nullptr;
	if (std::fwrite(data.data(), 1, data.size(), file) != data.size() ||
		std::fflush(file) != 0 || std::fseek(file, 0, SEEK_SET) != 0) {
		std::fclose(file);
		return nullptr;
	}
	return file;
}

} // namespace

extern "C" FILE *u7_opl3_score_open(int played_game, const char *fm_path,
	const char *mt_path, int kind)
{
	if ((played_game != 0 && played_game != 1) || !fm_path ||
		kind < U7_OPL3_SCORE_MUSIC_BANK || kind > U7_OPL3_SCORE_SI_EFFECTS ||
		(played_game == 0 && (kind == U7_OPL3_SCORE_SI_MENU || kind == U7_OPL3_SCORE_SI_EFFECTS)) ||
		(played_game == 1 && (kind == U7_OPL3_SCORE_BG_INTRO_BANK || kind == U7_OPL3_SCORE_BG_ENDING)))
		return nullptr;
	try {
		bool paired = kind == U7_OPL3_SCORE_MUSIC_BANK || kind == U7_OPL3_SCORE_BG_INTRO_BANK ||
			kind == U7_OPL3_SCORE_XMIDI_PAIR;
		auto fmPath = std::filesystem::absolute(fm_path).lexically_normal();
		auto mtPath = paired && mt_path ? std::filesystem::absolute(mt_path).lexically_normal() :
			std::filesystem::path{};
		auto &cache = scoreCache();
		std::lock_guard lock(cache.mutex);
		ScoreSources sources = scoreSources(played_game, fmPath, mtPath, kind);
		auto entry = std::find_if(cache.entries.begin(), cache.entries.end(), [&](const CachedScore &score) {
			return score.game == played_game && score.kind == kind &&
				score.sources.fm.path == fmPath && score.sources.mt.path == mtPath;
		});
		if (entry != cache.entries.end()) {
			if (entry->sources == sources) {
				cache.entries.splice(cache.entries.end(), cache.entries, entry);
				return scoreStream(entry->output);
			}
			cache.bytes -= entry->output.size();
			cache.entries.erase(entry);
		}
		Bytes fm, mt;
		if (!readFile(fmPath.string().c_str(), fm) ||
			(paired && !readFile(mtPath.string().c_str(), mt)))
			return nullptr;
		TimbreCoverage coverage(played_game, fmPath.string().c_str());
		Bytes out;
		try { out = transform(played_game, fm, mt, kind, coverage); }
		catch (const InvalidScore &) {}
		if (!sources.fm.exists || (paired && !sources.mt.exists) || out.size() > MaxCacheBytes ||
			sources != scoreSources(played_game, fmPath, mtPath, kind))
			return scoreStream(out);
		// Keep the converted bank across song opens, including unchanged results.
		while (!cache.entries.empty() && (cache.entries.size() >= MaxCachedScores ||
			out.size() > MaxCacheBytes - cache.bytes)) {
			cache.bytes -= cache.entries.front().output.size();
			cache.entries.pop_front();
		}
		cache.entries.push_back({played_game, kind, std::move(sources), std::move(out)});
		cache.bytes += cache.entries.back().output.size();
		return scoreStream(cache.entries.back().output);
	} catch (...) {
		return nullptr;
	}
}
