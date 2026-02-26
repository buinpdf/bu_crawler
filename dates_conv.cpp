#include "dates_conv.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <boost/multiprecision/cpp_int.hpp>

constexpr auto EMPTY_CJDN = -1;
constexpr auto MIN_CJDN_VALUE = 1721791;
const char* invalid_date = "ошибка определения даты";

using big_int = boost::multiprecision::cpp_int;
using INT = big_int;

namespace dates_conv {

big_int string_to_big_int(const std::string& i)
{
  big_int res;
  try { res.assign(i); }
  catch(const std::exception& e) {
    throw  std::runtime_error("ошибка преобразования строки '"+i+"' в число");
  }
  return res;
}

big_int string_to_year(const std::string& i)
{
  auto res = string_to_big_int(i);
  if( res < MIN_YEAR_VALUE )
    throw std::out_of_range("выход числа года '"+res.str()+"' за границу диапазона");
  return res;
}

bool is_leap_year(const Year& y, const CalendarFormat fmt)
{
  big_int year { string_to_big_int(y) };
  switch(fmt){
    case Grigorian: return (year%400 == 0) || (year%100 != 0 && year%4 == 0) ;
    case Julian: return (year%4 == 0) ;
    case Milankovic: {
      if(year%4 == 0) {
        if(year%100 == 0) {
          int x = boost::multiprecision::integer_modulus(year/100, 9);
          if(x == 2 || x == 6) return true;
          else return false;
        }
        return true;
      }
      return false;
    }
    default: return false;
  }
}

Day month_length(const Month month, const bool leap)
{
  switch(month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
        break;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
        break;
    case 2:
        return leap ? 29 : 28;
        break;
    default:
        return 0;
  }
}

/*----------------------------------------------*/
/*              class Date::impl                */
/*----------------------------------------------*/

class Date::impl {
  INT cjdn_;                         //Chronological Julian Day Number
  std::tuple<Year,Month,Day> gdate_; //Grigorian date
  std::tuple<Year,Month,Day> jdate_; //Julian date
  std::tuple<Year,Month,Day> mdate_; //Milankovic date

  int fdiv_(int a, int b) const;
  INT fdiv_(const INT& a, const INT& b) const;
  template<typename Integer>
    Integer mod_(const Integer& a, const Integer& b) const;
  std::pair<int,int> pdiv_(int a, int b) const;
  std::pair<INT,INT> pdiv_(const INT& a, const INT& b) const;
  INT grigorian2cjdn(const Year& y, const Month m, const Day d) const;
  INT julian2cjdn(const Year& y, const Month m, const Day d) const;
  INT milankovic2cjdn(const Year& y, const Month m, const Day d) const;
  std::tuple<Year,Month,Day> cjdn2grigorian(const INT& cjdn) const;
  std::tuple<Year,Month,Day> cjdn2julian(const INT& cjdn) const;
  std::tuple<Year,Month,Day> cjdn2milankovic(const INT& cjdn) const;

public:
  impl();
  impl(const Year& y, const Month m, const Day d, const CalendarFormat f);
  impl(const INT& cjdn);
  bool reset();
  bool reset(const Year& y, const Month m, const Day d, const CalendarFormat f);
  bool reset(const INT& new_cjdn);
  bool operator==(const Date::impl& rhs) const;
  bool operator!=(const Date::impl& rhs) const;
  bool operator<(const Date::impl& rhs) const ;
  bool operator>(const Date::impl& rhs) const ;
  bool operator>=(const Date::impl& rhs) const;
  bool operator<=(const Date::impl& rhs) const;
  bool is_valid() const;
  Year year(const CalendarFormat fmt) const;
  Month month(const CalendarFormat fmt) const;
  Day day(const CalendarFormat fmt) const;
  Weekday weekday() const;
  std::tuple<Year,Month,Day> ymd(const CalendarFormat fmt) const;
  INT cjdn() const;
  INT cjdn_from_incremented_by(unsigned long long c) const;
  INT cjdn_from_decremented_by(unsigned long long c) const;
  std::string& format(std::string& fmt) const;
};

bool Date::impl::reset()
{
  gdate_ = std::make_tuple<Year,Month,Day>({},{},{});
  jdate_ = std::make_tuple<Year,Month,Day>({},{},{});
  mdate_ = std::make_tuple<Year,Month,Day>({},{},{});
  cjdn_ = EMPTY_CJDN;
  return true;
}

bool Date::impl::reset(const Year& y, const Month m, const Day d, const CalendarFormat f)
{
  if( m<1 || m>12 ) return false;
  INT x;
  try { x.assign(y); } catch(const std::exception& e) { return false; }
  if( x < MIN_YEAR_VALUE ) return false;
  if( d<1 || d > month_length(m, is_leap_year(y, f)) ) return false;
  std::tuple<Year,Month,Day> jx, gx, mx ;
  switch(f) {
    case Grigorian: {
      x = grigorian2cjdn(y, m, d);
      gx = std::make_tuple(y, m, d);
      jx = cjdn2julian(x);
      mx = cjdn2milankovic(x);
    } break;
    case Julian: {
      x = julian2cjdn(y, m, d);
      jx = std::make_tuple(y, m, d);
      gx = cjdn2grigorian(x);
      mx = cjdn2milankovic(x);
    } break;
    case Milankovic: {
      x = milankovic2cjdn(y, m, d);
      mx = std::make_tuple(y, m, d);
      gx = cjdn2grigorian(x);
      jx = cjdn2julian(x);
    } break;
    default: { return false; }
  }
  INT jy ( std::get<0>(jx) );
  INT gy ( std::get<0>(gx) );
  INT my ( std::get<0>(mx) );
  if( jy < MIN_YEAR_VALUE || gy < MIN_YEAR_VALUE || my < MIN_YEAR_VALUE ) return false;
  gdate_ = gx;
  jdate_ = jx;
  mdate_ = mx;
  cjdn_  = x;
  return true;
}

bool Date::impl::reset(const INT& new_cjdn)
{
  if(new_cjdn == EMPTY_CJDN) {
    cjdn_  = EMPTY_CJDN ;
    gdate_ = std::make_tuple<Year,Month,Day>({},{},{});
    jdate_ = std::make_tuple<Year,Month,Day>({},{},{});
    mdate_ = std::make_tuple<Year,Month,Day>({},{},{});
  } else {
    if(new_cjdn < MIN_CJDN_VALUE) return false;
    auto jx = cjdn2julian(new_cjdn);
    INT jy ( std::get<0>(jx) );
    if( jy < MIN_YEAR_VALUE ) return false;
    auto gx = cjdn2grigorian(new_cjdn);
    INT gy ( std::get<0>(gx) );
    if( gy < MIN_YEAR_VALUE ) return false;
    auto mx = cjdn2milankovic(new_cjdn);
    INT my ( std::get<0>(mx) );
    if( my < MIN_YEAR_VALUE ) return false;
    gdate_ = gx;
    jdate_ = jx;
    mdate_ = mx;
    cjdn_ = new_cjdn;
  }
  return true;
}

Date::impl::impl()
{
  reset();
}

Date::impl::impl(const Year& y, const Month m, const Day d, const CalendarFormat f)
{
  if(!reset(y, m, d, f))
    throw std::runtime_error(std::string(invalid_date)+" '"+y+'.'+std::to_string(m)+'.'+std::to_string(d)+'\'');
}

Date::impl::impl(const INT& cjdn)
{
  if(!reset(cjdn))
    throw std::runtime_error(std::string(invalid_date)+" : cjdn = "+cjdn.str());
}

int Date::impl::fdiv_(int a, int b) const
{//floor division
  return (a - (a < 0 ? b - 1 : 0)) / b;
}

INT Date::impl::fdiv_(const INT& a, const INT& b) const
{//floor division
  if(a==0) return 0;
  INT quotient, remainder;
  boost::multiprecision::divide_qr(a, b, quotient, remainder);
  if(remainder==0) return quotient;
  if(quotient==0 && remainder<0) return -1;
  if(quotient==0 && remainder>0) return 0;
  if(quotient<0) return quotient-1;
  else return quotient;
}

template<typename Integer>
  Integer Date::impl::mod_(const Integer& a, const Integer& b) const
{
  return a - fdiv_(a, b) * b;
}

std::pair<int,int> Date::impl::pdiv_(int a, int b) const
{//positive remainder division
  std::div_t rv = std::div(a, b);
  if(rv.rem < 0) {
      if(b>0) {
          rv.quot -= 1;
          rv.rem += b;
      } else {
          rv.quot += 1;
          rv.rem -= b;
      }
  }
  return {rv.quot, rv.rem};
}

std::pair<INT,INT> Date::impl::pdiv_(const INT& a, const INT& b) const
{//positive remainder division
  INT quotient, remainder;
  boost::multiprecision::divide_qr(a, b, quotient, remainder);
  if(remainder < 0) {
      if(b>0) {
          quotient -= 1;
          remainder += b;
      } else {
          quotient += 1;
          remainder -= b;
      }
  }
  return {quotient, remainder};
}

INT Date::impl::grigorian2cjdn(const Year& y, const Month m, const Day d) const
// Dr Louis Strous's method:
// https://aa.quae.nl/en/reken/juliaansedag.html#3_1
{
  INT year = string_to_big_int(y);
  int c0 = fdiv_((m - 3) , 12);
  INT x1 = INT(m) - INT(12) * INT(c0) - INT(3);
  INT x4 = year + c0;
  auto [x3, x2] = pdiv_(x4, INT(100));
  INT result = d + 1721119;
  result += fdiv_( INT(146097) * x3, INT(4) ) ;
  result += fdiv_( INT(36525) * x2, INT(100) ) ;
  result += fdiv_( INT(153) * x1 + INT(2), INT(5) ) ;
  return result;
}

INT Date::impl::julian2cjdn(const Year& y, const Month m, const Day d) const
// Dr Louis Strous's method:
// https://aa.quae.nl/en/reken/juliaansedag.html#5_1
{
  INT year = string_to_big_int(y);
  int c0 = fdiv_((m - 3) , 12);
  INT j1 = fdiv_(INT(1461) * (year + INT(c0)), INT(4));
  int j2 = fdiv_(153 * m - 1836 * c0 - 457, 5);
  INT result = j1 + j2 + d + 1721117;
  return result;
}

INT Date::impl::milankovic2cjdn(const Year& y, const Month m, const Day d) const
// Dr Louis Strous's method:
// https://aa.quae.nl/en/reken/juliaansedag.html#4_1
{
  INT year = string_to_big_int(y);
  int c0 = fdiv_((m - 3) , 12);
  INT x4 = year + c0;
  INT x3 = fdiv_(x4, INT(100));
  int x2 = static_cast<int>(mod_(x4, INT(100)));
  int x1 = m - c0*12 - 3;
  INT result = d + 1721119;
  result += fdiv_( INT(328718) * x3 + INT(6), INT(9) ) ;
  result += fdiv_( INT(36525) * x2, INT(100) ) ;
  result += fdiv_( 153 * x1 + 2, 5 ) ;
  return result;
}

std::tuple<Year,Month,Day> Date::impl::cjdn2grigorian(const INT& cjdn) const
// Dr Louis Strous's method:
// https://aa.quae.nl/en/reken/juliaansedag.html#3_2
{
  auto [x3, r3] = pdiv_( INT(4) * cjdn - INT(6884477), INT(146097) ) ;
  auto [x2, r2] = pdiv_( 100 * fdiv_(static_cast<int>(r3), 4) + 99, 36525 ) ;
  auto [x1, r1] = pdiv_( 5 * fdiv_(r2, 100) + 2, 153 ) ;
  int c0 = fdiv_(x1 + 2, 12);
  Day d = fdiv_(r1, 5) + 1;
  Month m = x1 - 12 * c0 + 3;
  INT y = x3*100 + x2 + c0;
  return std::make_tuple(y.str(), m, d);
}

std::tuple<Year,Month,Day> Date::impl::cjdn2julian(const INT& cjdn) const
// Dr Louis Strous's method:
// https://aa.quae.nl/en/reken/juliaansedag.html#5_2
{
  INT y2 = cjdn - 1721118;
  INT k2 = y2*4 + 3;
  int k1 = 5 * fdiv_(static_cast<int>(mod_(k2, INT(1461))), 4) + 2;
  int x1 = fdiv_(k1, 153);
  int c0 = fdiv_(x1 + 2, 12);
  INT y = fdiv_(k2, INT(1461)) + c0;
  Month m = x1 - 12 * c0 + 3;
  Day d = fdiv_(mod_(k1, 153), 5) + 1;
  return std::make_tuple(y.str(), m, d);
}

std::tuple<Year,Month,Day> Date::impl::cjdn2milankovic(const INT& cjdn) const
// Dr Louis Strous's method:
// https://aa.quae.nl/en/reken/juliaansedag.html#4_2
{
  INT k3 = INT(9) * (cjdn - INT(1721120)) + 2;
  INT x3 = fdiv_(k3, INT(328718));
  int k2 = 100 * fdiv_(static_cast<int>(mod_(k3, INT(328718))), 9) + 99;
  int x2 = fdiv_(k2, 36525);
  int k1 = fdiv_(mod_(k2, 36525), 100) * 5 + 2;
  int x1 = fdiv_(k1, 153);
  int c0 = fdiv_(x1 + 2, 12);
  INT y = x3*100 + x2 + c0;
  Month m = x1 - 12 * c0 + 3;
  Day d = fdiv_(mod_(k1, 153), 5) + 1;
  return std::make_tuple(y.str(), m, d);
}

bool Date::impl::operator==(const Date::impl& rhs) const
{
  return cjdn_==rhs.cjdn_;
}

bool Date::impl::operator!=(const Date::impl& rhs) const
{
  return !(*this==rhs);
}

bool Date::impl::operator<(const Date::impl& rhs) const
{
  return cjdn_<rhs.cjdn_;
}

bool Date::impl::operator<=(const Date::impl& rhs) const
{
  return cjdn_<=rhs.cjdn_;
}

bool Date::impl::operator>(const Date::impl& rhs) const
{
  return cjdn_>rhs.cjdn_;
}

bool Date::impl::operator>=(const Date::impl& rhs) const
{
  return cjdn_>=rhs.cjdn_;
}

bool Date::impl::is_valid() const
{
  return cjdn_ != EMPTY_CJDN;
}

Year Date::impl::year(const CalendarFormat fmt) const
{
  Year result {};
  switch(fmt){
    case Grigorian: {
      result = std::get<0>(gdate_);
    } break;
    case Julian: {
      result = std::get<0>(jdate_);
    } break;
    case Milankovic: {
      result = std::get<0>(mdate_);
    } break;
    default: {}
  }
  return result;
}

Month Date::impl::month(const CalendarFormat fmt) const
{
  Month result {};
  switch(fmt){
    case Grigorian: {
      result = std::get<1>(gdate_);
    } break;
    case Julian: {
      result = std::get<1>(jdate_);
    } break;
    case Milankovic: {
      result = std::get<1>(mdate_);
    } break;
    default: {}
  }
  return result;
}

Day Date::impl::day(const CalendarFormat fmt) const
{
  Day result {};
  switch(fmt){
    case Grigorian: {
      result = std::get<2>(gdate_);
    } break;
    case Julian: {
      result = std::get<2>(jdate_);
    } break;
    case Milankovic: {
      result = std::get<2>(mdate_);
    } break;
    default: {}
  }
  return result;
}

Weekday Date::impl::weekday() const
{
  if(!is_valid()) return -1;
  return boost::multiprecision::integer_modulus(cjdn_ + 1, 7);
}

std::tuple<Year,Month,Day> Date::impl::ymd(const CalendarFormat fmt) const
{
  switch(fmt) {
    case Grigorian:  return gdate_ ;
    case Julian:     return jdate_ ;
    case Milankovic: return mdate_ ;
  }
  return std::make_tuple<Year,Month,Day>({},{},{}) ;
}

INT Date::impl::cjdn() const
{
  return cjdn_ ;
}

INT Date::impl::cjdn_from_incremented_by(unsigned long long c) const
{
  return cjdn_ + c;
}

INT Date::impl::cjdn_from_decremented_by(unsigned long long c) const
{
  return cjdn_ - c;
}

std::string& Date::impl::format(std::string& fmt) const
{
  if(fmt.size() < 3) return fmt;
  std::string gy_ = std::get<0>(gdate_);
  std::string gm_ = std::to_string(std::get<1>(gdate_));
  std::string gd_ = std::to_string(std::get<2>(gdate_));
  std::string jy_ = std::get<0>(jdate_);
  std::string jm_ = std::to_string(std::get<1>(jdate_));
  std::string jd_ = std::to_string(std::get<2>(jdate_));
  std::string my_ = std::get<0>(mdate_);
  std::string mm_ = std::to_string(std::get<1>(mdate_));
  std::string md_ = std::to_string(std::get<2>(mdate_));
  auto replacement = [this, &jy_, &gy_, &my_, &jm_, &gm_, &mm_, &jd_, &gd_, &md_](const std::string& c)->std::string{
    if(c=="%%")      { return "%"; }
    else if(c=="JY") { return jy_; }
    else if(c=="GY") { return gy_; }
    else if(c=="MY") { return my_; }
    else if(c=="Jq") { return jm_; }
    else if(c=="Gq") { return gm_; }
    else if(c=="Mq") { return mm_; }
    else if(c=="Jd") { return jd_; }
    else if(c=="Gd") { return gd_; }
    else if(c=="Md") { return md_; }
    else if(c=="Jy") { return jy_.size()<3 ? jy_ : jy_.substr(jy_.size()-2); }
    else if(c=="Gy") { return gy_.size()<3 ? gy_ : gy_.substr(gy_.size()-2); }
    else if(c=="My") { return my_.size()<3 ? my_ : my_.substr(my_.size()-2); }
    else if(c=="JM") { return jm_.empty() ? jm_ : Date::month_name(std::stoi(jm_)); }
    else if(c=="GM") { return gm_.empty() ? gm_ : Date::month_name(std::stoi(gm_)); }
    else if(c=="MM") { return mm_.empty() ? mm_ : Date::month_name(std::stoi(mm_)); }
    else if(c=="Jm") { return jm_.empty() ? jm_ : Date::month_short_name(std::stoi(jm_)); }
    else if(c=="Gm") { return gm_.empty() ? gm_ : Date::month_short_name(std::stoi(gm_)); }
    else if(c=="Mm") { return mm_.empty() ? mm_ : Date::month_short_name(std::stoi(mm_)); }
    else if(c=="JF") { return jm_.empty() ? jm_ : Date::month_name(std::stoi(jm_), false); }
    else if(c=="GF") { return gm_.empty() ? gm_ : Date::month_name(std::stoi(gm_), false); }
    else if(c=="MF") { return mm_.empty() ? mm_ : Date::month_name(std::stoi(mm_), false); }
    else if(c=="JQ") { return jm_.size()==1 ? ('0' + jm_) : jm_ ; }
    else if(c=="GQ") { return gm_.size()==1 ? ('0' + gm_) : gm_ ; }
    else if(c=="MQ") { return mm_.size()==1 ? ('0' + mm_) : mm_ ; }
    else if(c=="JD") { return jd_.size()==1 ? ('0' + jd_) : jd_ ; }
    else if(c=="GD") { return gd_.size()==1 ? ('0' + gd_) : gd_ ; }
    else if(c=="MD") { return md_.size()==1 ? ('0' + md_) : md_ ; }
    else if(c=="wd") { return std::to_string(weekday()); }
    else if(c=="WD") { return Date::weekday_name(weekday()); }
    else if(c=="Wd") { return Date::weekday_short_name(weekday()); }
    return '%' + c;
  };
  for(std::string::size_type pos{}; (pos=fmt.find('%', pos)) != fmt.npos; ) {
    if(pos == fmt.size()-1 || pos == fmt.size()-2) return fmt;
    auto repl = replacement(fmt.substr(pos+1, 2));
    fmt.replace(pos, 3, repl);
    pos += repl.size();
  }
  return fmt;
}

/*----------------------------------------------*/
/*                  class Date                  */
/*----------------------------------------------*/

/*static*/std::string Date::month_name(Month m, bool rp)
{
  if(rp) {
    switch(m) {
      case 1: { return "Января"; }
      case 2: { return "Февраля"; }
      case 3: { return "Марта"; }
      case 4: { return "Апреля"; }
      case 5: { return "Мая"; }
      case 6: { return "Июня"; }
      case 7: { return "Июля"; }
      case 8: { return "Августа"; }
      case 9: { return "Сентября"; }
      case 10:{ return "Октября"; }
      case 11:{ return "Ноября"; }
      case 12:{ return "Декабря"; }
    };
  } else {
    switch(m) {
      case 1: { return "Январь"; }
      case 2: { return "Февраль"; }
      case 3: { return "Март"; }
      case 4: { return "Апрель"; }
      case 5: { return "Май"; }
      case 6: { return "Июнь"; }
      case 7: { return "Июль"; }
      case 8: { return "Август"; }
      case 9: { return "Сентябрь"; }
      case 10:{ return "Октябрь"; }
      case 11:{ return "Ноябрь"; }
      case 12:{ return "Декабрь"; }
    };
  }
  return {};
}

/*static*/std::string Date::month_short_name(Month m)
{
  switch(m) {
    case 1: { return "янв"; }
    case 2: { return "фев"; }
    case 3: { return "мар"; }
    case 4: { return "апр"; }
    case 5: { return "мая"; }
    case 6: { return "июн"; }
    case 7: { return "июл"; }
    case 8: { return "авг"; }
    case 9: { return "сен"; }
    case 10:{ return "окт"; }
    case 11:{ return "ноя"; }
    case 12:{ return "дек"; }
  };
  return {};
}

/*static*/std::string Date::weekday_name(Weekday w)
{
  switch(w) {
    case 0: { return "Воскресенье"; }
    case 1: { return "Понедельник"; }
    case 2: { return "Вторник"; }
    case 3: { return "Среда"; }
    case 4: { return "Четверг"; }
    case 5: { return "Пятница"; }
    case 6: { return "Суббота"; }
  };
  return {};
}

/*static*/std::string Date::weekday_short_name(Weekday w)
{
  switch(w) {
    case 0: { return "Вс"; }
    case 1: { return "Пн"; }
    case 2: { return "Вт"; }
    case 3: { return "Ср"; }
    case 4: { return "Чт"; }
    case 5: { return "Пт"; }
    case 6: { return "Сб"; }
  };
  return {};
}

/*static*/bool Date::check(const Year& y, const Month m, const Day d, const CalendarFormat fmt)
{
  try
  {
    auto p = std::make_unique<Date::impl>(y, m, d, fmt) ;
  }
  catch(const std::exception& e)
  {
    return false;
  }
  return true;
}

/*static*/bool Date::check(const unsigned long long y, const Month m, const Day d, const CalendarFormat fmt)
{
  return check(std::to_string(y), m, d, fmt);
}

Date::Date() : pimpl(new Date::impl())
{
}

Date::Date(const Year& y, const Month m, const Day d, const CalendarFormat fmt)
  : pimpl(new Date::impl(y, m, d, fmt))
{
}

Date::Date(const unsigned long long y, const Month m, const Day d, const CalendarFormat fmt)
  : pimpl(new Date::impl(std::to_string(y), m, d, fmt))
{
}

Date::Date(const Date& other) : pimpl(new Date::impl(*other.pimpl))
{
}

Date& Date::operator=(const Date& other)
{
  if(this != &other) pimpl.reset(new Date::impl(*other.pimpl));
  return *this;
}

Date::Date(Date&&) noexcept = default ;

Date& Date::operator=(Date&&) noexcept = default ;

Date::~Date() = default;

bool Date::operator==(const Date& rhs) const
{
  return *pimpl == *rhs.pimpl;
}

bool Date::operator!=(const Date& rhs) const
{
  return *pimpl != *rhs.pimpl;
}

bool Date::operator<(const Date& rhs) const
{
  return *pimpl < *rhs.pimpl;
}

bool Date::operator<=(const Date& rhs) const
{
  return *pimpl <= *rhs.pimpl;
}

bool Date::operator>(const Date& rhs) const
{
  return *pimpl > *rhs.pimpl;
}

bool Date::operator>=(const Date& rhs) const
{
  return *pimpl >= *rhs.pimpl;
}

bool Date::empty() const
{
  return !pimpl->is_valid();
}

bool Date::is_valid() const
{
  return pimpl->is_valid();
}

Date::operator bool() const
{
  return pimpl->is_valid();
}

Year Date::year(const CalendarFormat fmt) const
{
  return pimpl->year(fmt);
}

Month Date::month(const CalendarFormat fmt) const
{
  return pimpl->month(fmt);
}

Day Date::day(const CalendarFormat fmt) const
{
  return pimpl->day(fmt);
}

Weekday Date::weekday() const
{
  return pimpl->weekday();
}

std::tuple<Year,Month,Day> Date::ymd(const CalendarFormat fmt) const
{
  return pimpl->ymd(fmt);
}

Date Date::inc_by_days(unsigned long long c) const
{
  try
  {
    auto new_pimpl = std::make_unique<Date::impl>(pimpl->cjdn_from_incremented_by(c)) ;
    Date result;
    result.pimpl.swap(new_pimpl);
    return result;
  }
  catch(const std::exception& e)
  {
    return {};
  }
}

Date Date::dec_by_days(unsigned long long c) const
{
  try
  {
    auto new_pimpl = std::make_unique<Date::impl>(pimpl->cjdn_from_decremented_by(c)) ;
    Date result;
    result.pimpl.swap(new_pimpl);
    return result;
  }
  catch(const std::exception& e)
  {
    return {};
  }
}

bool Date::reset(const Year& y, const Month m, const Day d, const CalendarFormat fmt)
{
  if(pimpl->reset(y, m, d, fmt)) return true;
  else return false;
}

bool Date::reset(const unsigned long long y, const Month m, const Day d, const CalendarFormat fmt)
{
  return reset(std::to_string(y), m, d, fmt);
}

std::string Date::format(std::string fmt) const
{
  return pimpl->format(fmt);
}

} // namespace dates_conv
