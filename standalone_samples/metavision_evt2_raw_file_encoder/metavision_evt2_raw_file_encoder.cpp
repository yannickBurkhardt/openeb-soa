/**********************************************************************************************************************
 * Copyright (c) Prophesee S.A.                                                                                       *
 *                                                                                                                    *
 * Licensed under the Apache License, Version 2.0 (the "License");                                                    *
 * you may not use this file except in compliance with the License.                                                   *
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0                                 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed   *
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                      *
 * See the License for the specific language governing permissions and limitations under the License.                 *
 **********************************************************************************************************************/

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdint>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <stdexcept>

namespace Metavision {
namespace Evt2 {

enum class EventTypes : uint8_t {
    CD_OFF        = 0x00, // OFF CD event, decrease in illumination (polarity '0')
    CD_ON         = 0x01, // ON CD event, increase in illumination (polarity '1')
    EVT_TIME_HIGH = 0x08, // Encodes the higher portion of the timebase (bits 33..6). Since it encodes the 28 higher
                          // bits over the 34 used to encode a timestamp, it has a resolution of 64us (= 2^(34-28)) and
                          // it can encode time values from 0us to 17179869183us (~ 4h46m20s). After
                          // 17179869120us its value wraps and returns to 0us.
    EXT_TRIGGER = 0x0A,   // External trigger output
};

// Evt2 raw events are 32-bit words
struct RawEvent {
    unsigned int pad : 28; // Padding
    unsigned int type : 4; // Event type
};

struct RawEventTime {
    unsigned int timestamp : 28; // Most significant bits of the event timestamp (bits 33..6)
    unsigned int type : 4;       // Event type: EventTypes::EVT_TIME_HIGH
};

struct RawEventCD {
    unsigned int y : 11;        // Pixel Y coordinate
    unsigned int x : 11;        // Pixel X coordinate
    unsigned int timestamp : 6; // Least significant bits of the event timestamp (bits 5..0)
    unsigned int type : 4;      // Event type: EventTypes::CD_OFF or EventTypes::CD_ON
};

struct RawEventExtTrigger {
    unsigned int value : 1; // Trigger current value (edge polarity):
                            // - '0' (falling edge);
                            // - '1' (rising edge).
    unsigned int unused2 : 7;
    unsigned int id : 5; // Trigger channel ID.
    unsigned int unused1 : 9;
    unsigned int timestamp : 6; // Least significant bits of the event timestamp (bits 5..0)
    unsigned int type : 4;      // Event type: EventTypes::EXT_TRIGGER
};

using Timestamp = uint64_t; // Type for timestamp, in microseconds

/// @brief Class that reads CD events from a CSV file and encodes them in EVT2 format
struct EventCDEncoder {
public:
    /// @brief Column position in the sensor at which the event happened
    uint16_t x;

    /// @brief Row position in the sensor at which the event happened
    uint16_t y;

    /// @brief Polarity
    ///
    /// The polarity represents the change of contrast
    ///     - 1: a positive contrast change
    ///     - 0: a negative contrast change
    int16_t p;

    /// @brief Timestamp at which the event happened (in us)
    Timestamp t;

    /// @brief Time origin subtracted from the timestamps read from the input file (in us)
    ///
    /// Input timestamps are assumed to be expressed on the same time base as this offset, so that the encoded
    /// timestamps end up relative to the start of the recording. Left at 0, input timestamps are encoded as-is.
    Timestamp t_offset = 0;

    /// @brief Set when a line could not be turned into an encodable event, so that the caller can fail cleanly
    bool has_error = false;

private:
    /// @brief Vector used to parse CSV input lines
    std::vector<std::string> tokens_;

    /// @brief Number of lines read so far, used to report the position of a malformed line
    size_t line_number_ = 0;

public:
    /// @brief Reads next line of CSV file
    /// @param ifs Stream to the input file to read
    bool read_next_line(std::ifstream &ifs) {
        std::string line;
        if (std::getline(ifs, line)) {
            ++line_number_;
            if (line.empty()) {
                return false; // Blank line, treat as end of input
            }
            std::istringstream iss(line);
            tokens_.clear();
            std::string token;
            while (std::getline(iss, token, ',')) {
                tokens_.push_back(token);
            }
            if (tokens_.size() != 4) {
                std::cerr << "Error: invalid line " << line_number_ << " for CD event, expected "
                          << "\"x,y,polarity,timestamp\", got: <" << line << ">" << std::endl;
                has_error = true;
                return false;
            }
            // The fields are parsed with the throwing std::sto* functions, so report a malformed line rather than
            // letting an uncaught exception abort the whole conversion
            Timestamp absolute_t = 0;
            try {
                x          = static_cast<uint16_t>(std::stoul(tokens_[0]));
                y          = static_cast<uint16_t>(std::stoul(tokens_[1]));
                p          = static_cast<int16_t>(std::stoi(tokens_[2]));
                absolute_t = static_cast<Timestamp>(std::stoll(tokens_[3]));
            } catch (const std::out_of_range &) {
                std::cerr << "Error: out of range value on line " << line_number_ << " for CD event: <" << line << ">"
                          << std::endl;
                has_error = true;
                return false;
            } catch (const std::invalid_argument &) {
                std::cerr << "Error: could not parse line " << line_number_ << " for CD event as "
                          << "\"x,y,polarity,timestamp\": <" << line << ">" << std::endl;
                has_error = true;
                return false;
            }
            if (absolute_t < t_offset) {
                std::cerr << "Error: CD event timestamp " << absolute_t << " us on line " << line_number_
                          << " predates the recording start time of " << t_offset << " us" << std::endl;
                has_error = true;
                return false;
            }
            t = absolute_t - t_offset;
            return true;
        }
        return false;
    }

    /// @brief Encodes CD event
    /// @param raw_event Pointer to the data to write
    void encode(RawEvent *raw_event) {
        RawEventCD *raw_cd_event = reinterpret_cast<RawEventCD *>(raw_event);
        raw_cd_event->x          = x;
        raw_cd_event->y          = y;
        raw_cd_event->timestamp  = t;
        raw_cd_event->type = p ? static_cast<uint8_t>(EventTypes::CD_ON) : static_cast<uint8_t>(EventTypes::CD_OFF);
    }
};

/// @brief Class that reads Trigger events from a CSV file and encodes them in EVT2 format
struct EventTriggerEncoder {
public:
    /// Polarity representing the change of contrast (1: positive, 0: negative)
    int16_t p;

    /// Timestamp at which the event happened (in us)
    Timestamp t;

    /// ID of the external trigger
    int16_t id;

    /// @brief Time origin subtracted from the timestamps read from the input file (in us)
    ///
    /// Input timestamps are assumed to be expressed on the same time base as this offset, so that the encoded
    /// timestamps end up relative to the start of the recording. Left at 0, input timestamps are encoded as-is.
    Timestamp t_offset = 0;

    /// @brief Set when a line could not be turned into an encodable event, so that the caller can fail cleanly
    bool has_error = false;

private:
    /// @brief Vector used to parse CSV input lines
    std::vector<std::string> tokens_;

    /// @brief Number of lines read so far, used to report the position of a malformed line
    size_t line_number_ = 0;

public:
    /// @brief Reads next line of CSV file
    /// @param ifs Stream to the input file to read
    bool read_next_line(std::ifstream &ifs) {
        std::string line;
        if (std::getline(ifs, line)) {
            ++line_number_;
            if (line.empty()) {
                return false; // Blank line, treat as end of input
            }
            std::istringstream iss(line);
            tokens_.clear();
            std::string token;
            while (std::getline(iss, token, ',')) {
                tokens_.push_back(token);
            }
            if (tokens_.size() != 3) {
                std::cerr << "Error: invalid line " << line_number_ << " for Trigger event, expected "
                          << "\"value,id,timestamp\", got: <" << line << ">" << std::endl;
                has_error = true;
                return false;
            }
            // The fields are parsed with the throwing std::sto* functions, so report a malformed line rather than
            // letting an uncaught exception abort the whole conversion
            Timestamp absolute_t = 0;
            try {
                p          = static_cast<int16_t>(std::stoi(tokens_[0]));
                id         = static_cast<int16_t>(std::stoi(tokens_[1]));
                absolute_t = static_cast<Timestamp>(std::stoll(tokens_[2]));
            } catch (const std::out_of_range &) {
                std::cerr << "Error: out of range value on line " << line_number_ << " for Trigger event: <" << line
                          << ">" << std::endl;
                has_error = true;
                return false;
            } catch (const std::invalid_argument &) {
                std::cerr << "Error: could not parse line " << line_number_ << " for Trigger event as "
                          << "\"value,id,timestamp\": <" << line << ">" << std::endl;
                has_error = true;
                return false;
            }
            if (absolute_t < t_offset) {
                std::cerr << "Error: trigger event timestamp " << absolute_t << " us on line " << line_number_
                          << " predates the recording start time of " << t_offset << " us" << std::endl;
                has_error = true;
                return false;
            }
            t = absolute_t - t_offset;
            return true;
        }
        return false;
    }

    /// @brief Encodes Trigger event
    /// @param raw_event Pointer to the data to write
    void encode(RawEvent *raw_event) {
        RawEventExtTrigger *raw_trigger_event = reinterpret_cast<RawEventExtTrigger *>(raw_event);
        raw_trigger_event->timestamp          = t;
        raw_trigger_event->id                 = id;
        raw_trigger_event->value              = p;
        raw_trigger_event->type               = static_cast<uint8_t>(EventTypes::EXT_TRIGGER);
    }
};

/// @brief Class that encodes Time High events in EVT2 format
struct EventTimeEncoder {
    /// @brief Constructor
    /// @param base Time (in us) of the first event to encode
    EventTimeEncoder(Timestamp base) : th((base / TH_NEXT_STEP) * TH_NEXT_STEP) {}

    /// @brief Encodes Time High
    /// @param raw_event Pointer to the data to write
    void encode(RawEvent *raw_event) {
        auto ev_th       = reinterpret_cast<RawEventTime *>(raw_event);
        ev_th->timestamp = th >> N_LOWER_BITS_TH;
        ev_th->type      = static_cast<uint8_t>(EventTypes::EVT_TIME_HIGH);
        th += TH_NEXT_STEP;
    }

    /// Next Time High to encode
    Timestamp th;

private:
    static constexpr char N_LOWER_BITS_TH           = 6;
    static constexpr unsigned int REDUNDANCY_FACTOR = 4;
    static constexpr Timestamp TH_STEP              = (1ul << N_LOWER_BITS_TH);
    static constexpr Timestamp TH_NEXT_STEP         = TH_STEP / REDUNDANCY_FACTOR;
};

} // namespace Evt2
} // namespace Metavision

/// @brief Structure containing metadata describing the input file
struct Metadata {
    int sensor_width  = 1280,
        sensor_height = 720; // Sensor width & height, by default assume PSEE Gen4 geometry (largest geometry)

    /// @brief Whether the geometry above was read from the input CSV header rather than left at its default
    bool geometry_from_csv = false;
};

bool read_cd_csv_header_line(std::ifstream &ifs, Metadata &metadata) {
    std::string line;
    if (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string key, value;
        std::vector<std::string> values;
        iss.ignore(1); // ignore leading '%'
        std::getline(iss, key, ':');
        while (std::getline(iss, value, ',')) {
            values.push_back(value);
        }
        if (key == "geometry") {
            if (values.size() == 2) {
                metadata.sensor_width      = std::stoi(values[0]);
                metadata.sensor_height     = std::stoi(values[1]);
                metadata.geometry_from_csv = true;
            } else {
                std::cerr
                    << "Ignoring invalid header line for key geometry, expected \"%geometry:<width>,<height>\", got: \""
                    << line << "\"" << std::endl;
            }
        }
    }
    return ifs.good();
}

bool read_cd_csv_header(std::ifstream &ifs, Metadata &metadata) {
    while (ifs.peek() == '%') {
        if (!read_cd_csv_header_line(ifs, metadata)) {
            return false;
        }
    }
    return true;
}

namespace {

/// @brief Prints the command line usage of this sample
/// @param program_name Name this sample was invoked with
void print_usage(const std::string &program_name) {
    std::cerr << std::endl
              << "Usage: " << program_name
              << " OUTPUT_FILENAME CD_INPUTFILE (TRIGGER_INPUTFILE) [--t-offset-us VALUE] [--geometry WIDTHxHEIGHT]"
              << std::endl;
    std::cerr << "Triggers will be encoded only if given trigger file has been given as input" << std::endl;
    std::cerr << std::endl << "Example: " << program_name << " output_file.raw cd_input.csv" << std::endl;
    std::cerr << "Example: " << program_name << " output_file.raw cd_input.csv --t-offset-us 1636017153123456"
              << std::endl;
    std::cerr << std::endl;
    std::cerr << "The CD CSV file needs to have the format: x,y,polarity,timestamp" << std::endl;
    std::cerr << "The Trigger input CSV file needs to have the format: value,id,timestamp" << std::endl;
    std::cerr << std::endl << "Options:" << std::endl;
    std::cerr << "  --t-offset-us VALUE       Time at which the recording started, as a Unix timestamp in integer"
              << std::endl;
    std::cerr << "                            microseconds. Written to the output header as \"% t_offset_us VALUE\","
              << std::endl;
    std::cerr << "                            and used to fill the \"% date\" line (formatted as UTC) instead of the"
              << std::endl;
    std::cerr << "                            current wall clock time." << std::endl;
    std::cerr << "  --geometry WIDTHxHEIGHT   Sensor geometry to write in the \"% format\" header line, e.g. 640x480."
              << std::endl;
    std::cerr << "                            Passing a geometry that contradicts the \"%geometry\" header line of the"
              << std::endl;
    std::cerr << "                            input CD CSV file is an error." << std::endl;
}

/// @brief Parses a non-negative integer, requiring the whole string to be consumed
/// @param value String to parse
/// @param parsed Parsed value, only meaningful if this function returns true
/// @return true if @p value is a valid non-negative integer
bool parse_non_negative_integer(const std::string &value, int64_t &parsed) {
    if (value.empty()) {
        return false;
    }
    try {
        size_t pos             = 0;
        const long long result = std::stoll(value, &pos);
        if (pos != value.size() || result < 0) {
            return false;
        }
        parsed = static_cast<int64_t>(result);
    } catch (const std::invalid_argument &) {
        return false;
    } catch (const std::out_of_range &) {
        return false;
    }
    return true;
}

} // anonymous namespace

int main(int argc, char *argv[]) {
    // Parse the command line: positional arguments keep their historical meaning and order, and are optionally
    // followed (or preceded, or interleaved) by named options
    const std::string program_name(argv[0]);
    std::vector<std::string> positionals;
    bool t_offset_given = false;
    int64_t t_offset_us = 0;
    bool geometry_given = false;
    int cli_sensor_width = 0, cli_sensor_height = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg.rfind("--", 0) != 0) {
            positionals.push_back(arg);
            continue;
        }
        if (i + 1 >= argc) {
            std::cerr << "Error: option '" << arg << "' requires a value" << std::endl;
            print_usage(program_name);
            return 1;
        }
        const std::string value(argv[++i]);
        if (arg == "--t-offset-us") {
            if (!parse_non_negative_integer(value, t_offset_us)) {
                std::cerr << "Error: invalid value '" << value << "' for option '" << arg
                          << "': expected a non-negative integer number of microseconds since the Unix epoch"
                          << std::endl;
                return 1;
            }
            t_offset_given = true;
        } else if (arg == "--geometry") {
            static constexpr int64_t kMaxSensorDimension = 65535;
            const size_t separator_pos                   = value.find('x');
            int64_t width = 0, height = 0;
            if (separator_pos == std::string::npos || value.find('x', separator_pos + 1) != std::string::npos ||
                !parse_non_negative_integer(value.substr(0, separator_pos), width) ||
                !parse_non_negative_integer(value.substr(separator_pos + 1), height) || width <= 0 || height <= 0 ||
                width > kMaxSensorDimension || height > kMaxSensorDimension) {
                std::cerr << "Error: invalid value '" << value << "' for option '" << arg
                          << "': expected WIDTHxHEIGHT with positive integers up to " << kMaxSensorDimension
                          << ", e.g. 640x480" << std::endl;
                return 1;
            }
            cli_sensor_width  = static_cast<int>(width);
            cli_sensor_height = static_cast<int>(height);
            geometry_given    = true;
        } else {
            std::cerr << "Error: unknown option '" << arg << "'" << std::endl;
            print_usage(program_name);
            return 1;
        }
    }

    // Check input arguments validity
    if (positionals.size() < 2) {
        std::cerr << "Error: need output filename and input filename for CD events" << std::endl;
        print_usage(program_name);
        return 1;
    }
    if (positionals.size() > 3) {
        std::cerr << "Error: too many arguments, expected at most OUTPUT_FILENAME, CD_INPUTFILE and TRIGGER_INPUTFILE"
                  << std::endl;
        print_usage(program_name);
        return 1;
    }
    const std::string &output_filename = positionals[0];
    const std::string &cd_filename     = positionals[1];

    // Open input files
    std::ifstream input_cd_file(cd_filename);
    if (!input_cd_file.is_open()) {
        std::cerr << "Error: could not open file '" << cd_filename << "' for reading" << std::endl;
        return 1;
    }
    std::ifstream input_trigger_file;
    if (positionals.size() > 2) {
        input_trigger_file.open(positionals[2]);
        if (!input_trigger_file.is_open()) {
            std::cerr << "Error: could not open file '" << positionals[2] << "' for reading" << std::endl;
            return 1;
        }
    }

    // Check presence of header in input CD CSV file and if present, parse sensor geometry
    Metadata metadata;
    if (input_cd_file.peek() == '%') {
        if (!read_cd_csv_header(input_cd_file, metadata)) {
            std::cerr << "Error: error while reading csv header" << std::endl;
            return 1;
        }
    }

    // A geometry given on the command line overrides the default one, but must not contradict the input file
    if (geometry_given) {
        if (metadata.geometry_from_csv &&
            (cli_sensor_width != metadata.sensor_width || cli_sensor_height != metadata.sensor_height)) {
            std::cerr << "Error: geometry given on the command line (" << cli_sensor_width << "x" << cli_sensor_height
                      << ") contradicts the geometry declared in the header of '" << cd_filename << "' ("
                      << metadata.sensor_width << "x" << metadata.sensor_height << ")" << std::endl;
            return 1;
        }
        metadata.sensor_width  = cli_sensor_width;
        metadata.sensor_height = cli_sensor_height;
    }

    // Determine the date to stamp in the header: the given recording time if any, formatted as UTC, otherwise the
    // current wall clock time as local time. Done before opening the output file so a failure leaves no file behind.
    const std::time_t tt      = t_offset_given ? static_cast<std::time_t>(t_offset_us / 1000000) :
                                                 std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const struct std::tm *ptm = t_offset_given ? std::gmtime(&tt) : std::localtime(&tt);
    char date_str[32];
    if (ptm == nullptr || std::strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", ptm) == 0) {
        std::cerr << "Error: could not convert timestamp " << tt << " s to a calendar date" << std::endl;
        return 1;
    }

    // Open raw output file
    std::ofstream output_raw_file(output_filename, std::ios::binary);
    if (!output_raw_file.is_open()) {
        std::cerr << "Error: could not open file '" << output_filename << "' for writing" << std::endl;
        return 1;
    }

    // Write header: we write the header corresponding to Prophesee EVK3 Gen41 device (largest geometry)
    output_raw_file << "% date " << date_str << std::endl;
    output_raw_file << "% format EVT2;width=" << metadata.sensor_width << ";height=" << metadata.sensor_height
                    << std::endl;
    output_raw_file << "% integrator_name Prophesee" << std::endl;
    if (t_offset_given) {
        output_raw_file << "% t_offset_us " << t_offset_us << std::endl;
    }
    output_raw_file << "% end" << std::endl;

    // Initialize encoders
    Metavision::Evt2::EventCDEncoder CD_events_encoder;
    Metavision::Evt2::EventTriggerEncoder trigger_events_encoder;

    // Input timestamps share the time base of the given recording start time, so make them relative to it: the EVT2
    // time base starts at 0 and only spans about 4h46m, so absolute Unix timestamps cannot be encoded directly
    CD_events_encoder.t_offset      = static_cast<Metavision::Evt2::Timestamp>(t_offset_us);
    trigger_events_encoder.t_offset = static_cast<Metavision::Evt2::Timestamp>(t_offset_us);

    bool cd_done      = !CD_events_encoder.read_next_line(input_cd_file);
    bool trigger_done = input_trigger_file ? !trigger_events_encoder.read_next_line(input_trigger_file) : true;
    if (CD_events_encoder.has_error || trigger_events_encoder.has_error) {
        return 1;
    }
    if (cd_done && trigger_done) {
        std::cerr << "Error: no events in input file(s)" << std::endl;
        return 1;
    }

    // Start chrono
    const auto tp_start = std::chrono::system_clock::now();

    // Create a buffer where to store the encoded data before writing them in the output file
    static constexpr size_t kSizeBuffer = 1000;
    std::vector<Metavision::Evt2::RawEvent> raw_events(kSizeBuffer);
    Metavision::Evt2::RawEvent *raw_events_current_ptr = raw_events.data();
    Metavision::Evt2::RawEvent *raw_events_end_ptr     = raw_events_current_ptr + kSizeBuffer;

    // Determine the timestamp of the oldest event
    Metavision::Evt2::Timestamp first_ts = 0;
    if (!cd_done) {
        first_ts = CD_events_encoder.t;
    }
    if (!trigger_done && trigger_events_encoder.t < first_ts) {
        first_ts = trigger_events_encoder.t;
    }

    // Time High encoder
    Metavision::Evt2::EventTimeEncoder time_high_encoder(first_ts);
    // Encode First Time High
    time_high_encoder.encode(raw_events_current_ptr);
    ++raw_events_current_ptr;

    while (!(cd_done && trigger_done)) {
        if (raw_events_current_ptr == raw_events_end_ptr) {
            // Write in output file
            output_raw_file.write(reinterpret_cast<const char *>(raw_events.data()),
                                  kSizeBuffer * sizeof(Metavision::Evt2::RawEvent));
            raw_events_current_ptr = raw_events.data();
        }

        if (!cd_done) {
            if (CD_events_encoder.t < time_high_encoder.th) {
                if (!trigger_done && trigger_events_encoder.t < CD_events_encoder.t) {
                    // Encode Trigger Event
                    trigger_events_encoder.encode(raw_events_current_ptr);
                    trigger_done = !trigger_events_encoder.read_next_line(input_trigger_file);
                } else {
                    // Encode CD Event
                    CD_events_encoder.encode(raw_events_current_ptr);
                    cd_done = !CD_events_encoder.read_next_line(input_cd_file);
                }
            } else {
                if (!trigger_done && trigger_events_encoder.t < time_high_encoder.th) {
                    // Encode Trigger Event
                    trigger_events_encoder.encode(raw_events_current_ptr);
                    trigger_done = !trigger_events_encoder.read_next_line(input_trigger_file);
                } else {
                    // Encode TH
                    time_high_encoder.encode(raw_events_current_ptr);
                }
            }
        } else {
            // If we arrive here it means that trigger_done = false (cf while condition)
            if (trigger_events_encoder.t < time_high_encoder.th) {
                // Encode Trigger Event
                trigger_events_encoder.encode(raw_events_current_ptr);
                trigger_done = !trigger_events_encoder.read_next_line(input_trigger_file);
            } else {
                // Encode TH
                time_high_encoder.encode(raw_events_current_ptr);
            }
        }
        ++raw_events_current_ptr;
    }

    // Bail out rather than flush a stream that was cut short by an unencodable timestamp
    if (CD_events_encoder.has_error || trigger_events_encoder.has_error) {
        std::cerr << "Error: '" << output_filename << "' is incomplete" << std::endl;
        return 1;
    }

    // Write remaining encoded events in output file
    if (raw_events_current_ptr != raw_events.data()) {
        output_raw_file.write(reinterpret_cast<const char *>(raw_events.data()),
                              std::distance(raw_events.data(), raw_events_current_ptr) *
                                  sizeof(Metavision::Evt2::RawEvent));
    }

    // Display processing time
    const auto tp_end       = std::chrono::system_clock::now();
    const double duration_s = std::chrono::duration_cast<std::chrono::microseconds>(tp_end - tp_start).count() / 1e6;
    std::cout << "Encoded '" << output_filename << "' in " << duration_s << " s" << std::endl;

    return 0;
}
