#include "logging.h"
#include <boost/log/sinks/unlocked_frontend.hpp>
#include <boost/log/expressions.hpp>
#include <centiseconds.h>
#include "tools.h"

using std::string;
using std::lock_guard;
using boost::log::sinks::text_ostream_backend;
using boost::log::record_view;
using boost::log::sinks::unlocked_sink;
using std::vector;
using std::tuple;
using std::make_tuple;

namespace expr = boost::log::expressions;

template <>
const string& getEnumTypeName<LogLevel>() {
	static const string name = "LogLevel";
	return name;
}

template <>
const vector<tuple<LogLevel, string>>& getEnumMembers<LogLevel>() {
	static const vector<tuple<LogLevel, string>> values = {
		make_tuple(LogLevel::Trace,		"Trace"),
		make_tuple(LogLevel::Debug,		"Debug"),
		make_tuple(LogLevel::Info,		"Info"),
		make_tuple(LogLevel::Warning,	"Warning"),
		make_tuple(LogLevel::Error,		"Error"),
		make_tuple(LogLevel::Fatal,		"Fatal")
	};
	return values;
}

std::ostream& operator<<(std::ostream& stream, LogLevel value) {
	return stream << enumToString(value);
}

std::istream& operator>>(std::istream& stream, LogLevel& value) {
	string name;
	stream >> name;
	value = parseEnum<LogLevel>(name);
	return stream;
}

PausableBackendAdapter::PausableBackendAdapter(boost::shared_ptr<text_ostream_backend> backend) :
	backend(backend) {}

PausableBackendAdapter::~PausableBackendAdapter() {
	resume();
}

void PausableBackendAdapter::consume(const record_view& recordView, const string message) {
	lock_guard<std::mutex> lock(mutex);
	if (isPaused) {
		buffer.push_back(std::make_tuple(recordView, message));
	} else {
		backend->consume(recordView, message);
	}
}

void PausableBackendAdapter::pause() {
	lock_guard<std::mutex> lock(mutex);
	isPaused = true;
}

void PausableBackendAdapter::resume() {
	lock_guard<std::mutex> lock(mutex);
	isPaused = false;
	for (const auto& tuple : buffer) {
		backend->consume(std::get<record_view>(tuple), std::get<string>(tuple));
	}
	buffer.clear();
}

boost::shared_ptr<PausableBackendAdapter> initLogging() {
	// Create logging backend that logs to stderr
	auto streamBackend = boost::make_shared<text_ostream_backend>();
	streamBackend->add_stream(boost::shared_ptr<std::ostream>(&std::cerr, [](std::ostream*) {}));
	streamBackend->auto_flush(true);

	// Create an adapter that allows us to pause, buffer, and resume log output
	auto pausableAdapter = boost::make_shared<PausableBackendAdapter>(streamBackend);

	// Create a sink that feeds into the adapter
	auto sink = boost::make_shared<unlocked_sink<PausableBackendAdapter>>(pausableAdapter);

	// Set output formatting
	sink->set_formatter(expr::stream << "[" << expr::attr<LogLevel>("Severity") << "] " << expr::smessage);

	boost::log::core::get()->add_sink(sink);
	return pausableAdapter;
}

void logTimedEvent(const string& eventName, centiseconds start, centiseconds end, const string& value) {
	LOG_DEBUG << "##" << eventName << "[" << formatDuration(start) << "-" << formatDuration(end) << "]: " << value;
}
