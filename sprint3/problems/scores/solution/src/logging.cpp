#include "logging.h"

#include <boost/json.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>   // <--- добавлено
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>

namespace logging {
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(message, "Message", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

void InitBoostLog() {
    logging::core::get()->add_global_attribute("TimeStamp", attrs::local_clock());

    using text_sink = sinks::synchronous_sink<sinks::text_ostream_backend>;
    auto sink = boost::make_shared<text_sink>();
    sink->locked_backend()->add_stream(boost::make_shared<std::ostream>(std::cout.rdbuf()));
    sink->locked_backend()->auto_flush(true);
    sink->set_formatter([](const logging::record_view& rec, logging::formatting_ostream& strm) {
        boost::json::object obj;

        if (auto ts = rec[timestamp]) {
            obj["timestamp"] = boost::posix_time::to_iso_extended_string(ts.get());
        }

        if (auto data = rec[additional_data]) {
            obj["data"] = data.get();
        } else {
            obj["data"] = boost::json::object{};
        }

        if (auto msg = rec[message]) {
            obj["message"] = msg.get();
    	} else {
            obj["message"] = "**MISSING**";   // атрибут отсутствует
        }

        strm << boost::json::serialize(obj);
    });

    logging::core::get()->add_sink(sink);
}

void LogMessage(std::string_view message, boost::json::value data) {
    static logging::sources::severity_logger<> logger;
    logging::record rec = logger.open_record();
    if (rec) {
        logging::attribute_value_set& values = rec.attribute_values();
        values.insert("Message", 
                      logging::attributes::make_attribute_value(std::string(message)));
        values.insert("AdditionalData", 
                      logging::attributes::make_attribute_value(std::move(data)));
        logger.push_record(std::move(rec));
    }
}

}  // namespace logging