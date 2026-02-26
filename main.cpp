#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <chrono>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/nowide/args.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/json/src.hpp>
#include "root_certificates.hpp"
#include "dates_conv.h"
#include "lex.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace lx = pg::lex;
namespace nw = boost::nowide;
namespace dc = dates_conv;
namespace ch = std::chrono;
namespace js = boost::json;
using tcp = net::ip::tcp;

template <typename Arg, typename... Args> void THROW (Arg&& arg, Args&&... args)
{
  std::ostringstream out;
  out << std::forward<Arg>(arg);
  ((out << ' ' << std::forward<Args>(args)), ...);
  throw std::runtime_error(out.str());
}

int main(int argc, char** argv)
{
  nw::args a_ (argc, argv); // Fix arguments - make them UTF-8
  if(argc != 4)
  {
      nw::cout << "Usage:\n" << argv[0] << " <year> <out_bu_html_file> <out_calendar_html_file>" << std::endl;
      return EXIT_FAILURE;
  }

  const auto user_agent = "Mozilla/5.0 (X11; Linux x86_64; rv:145.0) Gecko/20100101 Firefox/145.0" ;
  const auto html_vertical_empty_space = "<p style=\"margin-bottom:2cm;margin-top:2cm;\">&nbsp;</p>";
  const auto err_cant_create = "can't create file: ";
  const auto host = "api.patriarchia.ru";
  const auto path = "/v1/events/"; // YYYY-MM-DD [in julian calendar]
  const auto port = "443";

  try
  {
    const ch::time_point now {ch::system_clock::now()};
    const ch::year_month_day ymd {ch::floor<ch::days>(now)};
    const dc::Date current_date {
      static_cast<unsigned>(static_cast<int>(ymd.year())),
      static_cast<int>(static_cast<unsigned>(ymd.month())),
      static_cast<int>(static_cast<unsigned>(ymd.day())),
      dc::Grigorian
    };

    std::vector<dc::Date> targets;
    for (int m = 1; m < 2; ++m)
      for (int d = 1; d <= 5/*dc::month_length(m, dc::is_leap_year(argv[1], dc::Julian))*/; ++d) {
        targets.emplace_back(argv[1], m, d, dc::Julian);
      }

    nw::ofstream out_bu (argv[2]);
      if (!out_bu.is_open()) THROW(err_cant_create, argv[2]);

    nw::ofstream out_ca (argv[3]);
      if (!out_ca.is_open()) THROW(err_cant_create, argv[3]);

    std::ostringstream os;
    os << "<!DOCTYPE html><html lang=\"ru-RU\"><head>"
          "<meta charset='UTF-8'><title>Богослужебные указания " << argv[1] <<
          "</title><style>td {padding: 4px;text-align:center;vertical-align:top;}"
          "</style></head>\n<body style=\"margin-top:1cm;"
          "margin-bottom:1cm;margin-left:1cm;margin-right:1cm;\">\n"
          "<h2>Богослужебные указания " << argv[1] << "</h2>\n"
          "<p>Документ сгенерирован автоматически " << current_date.format("%Gd %GM %GY г.") <<
          " (" << current_date.format("%Jd %JM %JY г.") << " по ст. ст.)"
          " на основе материалов сайта www.patriarchia.ru</p>\n" ;

    out_bu << os.view();
    out_ca << lx::gsub( os.view(), "Богослужебные указания", "Богослужебный календарь" );

    auto print_tables_for = [year=argv[1]](int first_month, int last_month, std::ostream& out){
      out << "<table><tr>" ;
      for (int m = first_month; m <= last_month; ++m) {
        out << "<td>" << dc::Date::month_name(m,0) << "<br><table><tr>" ;
        for (int d = 1; d <= dc::month_length(m, dc::is_leap_year(year, dc::Julian)); ++d) {
          out << "<td><a href=\"#bu-" << +m << '-' << +d << "\">" << +d << "</a></td>" ;
          if (d%5 == 0) out << "</tr><tr>";
        }
        out << "</tr></table></td>" ;
      }
      out << "</tr></table>\n" ;
    };

    out_bu << "<h3>Содержание (даты по ст. ст.)</h3>\n" ;
    print_tables_for(1, 3, out_bu);
    print_tables_for(4, 6, out_bu);
    print_tables_for(7, 9, out_bu);
    print_tables_for(10, 12, out_bu);
    out_bu << html_vertical_empty_space << '\n' ;

    // The io_context is required for all I/O
    net::io_context ioc;

    // The SSL context is required, and holds certificates
    ssl::context ctx(ssl::context::tlsv12_client);

    // This holds the root certificate used for verification
    load_root_certificates(ctx);

    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_peer);

    for(const auto& target: targets) {
      // These objects perform our I/O
      tcp::resolver resolver(ioc);
      ssl::stream<beast::tcp_stream> stream(ioc, ctx);
      // Set SNI Hostname (many hosts need this to handshake successfully)
      if(! SSL_set_tlsext_host_name(stream.native_handle(), host))
      {
          beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
          throw beast::system_error{ec};
      }
      // Look up the domain name
      auto const results = resolver.resolve(host, port);
      // Make the connection on the IP address we get from a lookup
      beast::get_lowest_layer(stream).connect(results);
      // Perform the SSL handshake
      stream.handshake(ssl::stream_base::client);
      // Set up an HTTP GET request message
      http::request<http::string_body> req {
        http::verb::get,
        std::string(path) + target.format(),
        11
      };
      req.set(http::field::host, host);
      req.set(http::field::user_agent, user_agent);

      // Send the HTTP request to the remote host
      http::write(stream, req);

      // This buffer is used for reading and must be persisted
      beast::flat_buffer buffer;

      // Declare a container to hold the response
      http::response<http::string_body> res;

      // Receive the HTTP response
      http::read(stream, buffer, res);
      if (res.result_int() != 200)
        THROW("HTTP Server error:\n\n" , res.base());

      // extract text from result message
      std::string bu_text, ca_text;
      try
      {
        js::value jv = js::parse( res.body() );
        const js::object& obj = jv.as_object();
        bu_text = js::value_to<std::string>( obj.at( "content" ) );
        ca_text = js::value_to<std::string>( obj.at( "calendar_text" ) );
      }
      catch(const std::exception&)
      {
        nw::cout << "content not found for date:" << target.format() << "\n\n" << res.base() << '\n' ;
        break ;
      }
      auto json_text_to_html = [](std::string_view in){
        std::string result = lx::gsub( in, "\\\\u(%x%x%x%x)", "&#x%1;" );
        result = lx::gsub( result, "\\\\/", "/" );
        result = lx::gsub( result, "\\r", "" );
        result = lx::gsub( result, "\\n", "" );
        result = lx::gsub( result, "<a[^>]+>([^<]+)</a>", "%1" );
        return result;
      };
      bu_text = json_text_to_html(bu_text);
      ca_text = json_text_to_html(ca_text);

      std::ostringstream os;
      os << "<strong>" << target.format("%Jd %JM %JY г. по ст. ст.") << "</strong> "
         << target.format("(%Gd %GM %GY г. по н. ст.) ") ;

      out_bu << "<p id=\"bu-" << +target.month() << '-' << +target.day() << "\">"
        << os.view() << "</p>\n" << bu_text << '\n' << html_vertical_empty_space << '\n' ;

      out_ca << "<hr><p>" << os.view() << target.format("%WD") << "</p>\n" << ca_text << '\n'
        << html_vertical_empty_space << '\n' ;
      stream.shutdown();
    }

    out_bu << "\n</body>\n</html>\n";
    out_ca << "\n</body>\n</html>\n";
    out_bu.close();
    out_ca.close();
    nw::cout << "Complete!\n" ;
  }
  catch(std::exception const& e)
  {
    nw::cout << "Exception: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch(...)
  {
    nw::cout << "Unknown exception" << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
