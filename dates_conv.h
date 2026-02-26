#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>

namespace dates_conv {

using Year = std::string;
using Month = int;
using Day = int;
using Weekday = int;

enum class CalendarFormat {
  J, ///< формат календаря: юлианский
  M, ///< формат календаря: ново-юлианский
  G  ///< формат календаря: григорианский
};
constexpr auto Julian = CalendarFormat::J;    ///< формат календаря: юлианский
constexpr auto Milankovic = CalendarFormat::M;///< формат календаря: ново-юлианский
constexpr auto Grigorian = CalendarFormat::G; ///< формат календаря: григорианский
constexpr auto MIN_YEAR_VALUE = 2;            ///< допустимый минимум для числа года

/**
  *  Функция возвращает true для высокосного года
  *
  *  \param [in] y число года
  *  \param [in] fmt выбор типа календаря для вычислений
  */
bool is_leap_year(const Year& y, const CalendarFormat fmt);

/**
  *  Функция возвращает кол-во дней в месяце
  *
  *  \param [in] month число месяца (1 - январь, 2 - февраль и т.д.)
  *  \param [in] leap признак высокосного года
  */
Day month_length(const Month month, const bool leap);

/**
 * Класс даты. Реализует преобразования между 3-мя календарными системами (григорианский, юлианский, ново-юлианский)
 * по методу Dr. Louis Strous'a - https://aa.quae.nl/en/reken/juliaansedag.html
 * Для числа года используется строковое представление. Конструктор принимающий строковое число года,
 * бросает исключение если строку невозможно преобразовать в целое число произвольной величины
 * или если число (во всех календарных форматах) < MIN_YEAR_VALUE.
 */
class Date {
  class impl;
  std::unique_ptr<impl> pimpl;
public:
  /**
    *  Возвращает название месяца
    *
    *  \param [in] m число месяца (1 - январь, 2 - февраль и т.д.)
    *  \param [in] rp название в род. падеже
    */
  static std::string month_name(Month m, bool rp=true);
  /**
    *  Возвращает сокращенное название месяца
    *
    *  \param [in] m число месяца (1 - январь, 2 - февраль и т.д.)
    */
  static std::string month_short_name(Month m);
  /**
    *  Возвращает название дня недели
    *
    *  \param [in] w число дня недели (0-вс, 1-пн, 2-вт ...)
    */
  static std::string weekday_name(Weekday w);
  /**
    *  Возвращает сокращенное название дня недели
    *
    *  \param [in] w число дня недели (0-вс, 1-пн, 2-вт ...)
    */
  static std::string weekday_short_name(Weekday w);
  /**
   *  Проверка даты на корректность
   *
   *  \param [in] y число года
   *  \param [in] m число месяца
   *  \param [in] d число дня
   *  \param [in] fmt тип календаря для даты
   */
  static bool check(const Year& y, const Month m, const Day d, const CalendarFormat fmt=Julian);
  /**
   *   Перегруженная версия. Отличается только типом параметров.
   */
  static bool check(const unsigned long long y, const Month m, const Day d, const CalendarFormat fmt=Julian);
  /**
    *  Конструктор
    */
  Date();
  /**
    *  Конструктор
    *
    *  \param [in] y число года
    *  \param [in] m число месяца
    *  \param [in] d число дня
    *  \param [in] fmt тип календаря для вх. даты
    */
  Date(const Year& y, const Month m, const Day d, const CalendarFormat fmt=Julian);
  /**
    *  Конструктор
    *
    *  \param [in] y число года
    *  \param [in] m число месяца
    *  \param [in] d число дня
    *  \param [in] fmt тип календаря для вх. даты
    */
  Date(const unsigned long long y, const Month m, const Day d, const CalendarFormat fmt=Julian);
  Date(const Date&);
  Date& operator=(const Date&);
  Date(Date&&) noexcept;
  Date& operator=(Date&&) noexcept;
  virtual ~Date();
  bool operator==(const Date&) const;
  bool operator!=(const Date&) const;
  bool operator<(const Date&) const;
  bool operator<=(const Date&) const;
  bool operator>(const Date&) const;
  bool operator>=(const Date&) const;
  /**
    *  Возвращает true если объект не содержит корректной даты
    */
  bool empty() const;
  /**
    *  Возвращает true если объект содержит корректную дату
    */
  bool is_valid() const;
  explicit operator bool() const;
  /**
    *  Извлекает значение года из даты для определенного типа календаря
    *
    *  \param [in] fmt тип календаря
    */
  Year year(const CalendarFormat fmt=Julian) const;
  /**
    *  Извлекает значение месяца из даты для определенного типа календаря
    *
    *  \param [in] fmt тип календаря
    */
  Month month(const CalendarFormat fmt=Julian) const;
  /**
    *  Извлекает число дня (в месяце) для определенного типа календаря
    *
    *  \param [in] fmt тип календаря
    */
  Day day(const CalendarFormat fmt=Julian) const;
   /**
    *  Извлекает день недели для даты. 0-вс, 1-пн, 2-вт, 3-ср, 4-чт, 5-пт, 6-сб.
    */
  Weekday weekday() const;
  /**
    *  Извлекает дату по типу календаря, в формате std::tuple
    *
    *  \param [in] fmt тип календаря
    */
  std::tuple<Year, Month, Day> ymd(const CalendarFormat fmt=Julian) const;
  /**
    *  Возвращает новую дату, увеличенную на кол-во дней от текущей
    *
    *  \param [in] c кол-во дней
    */
  Date inc_by_days(unsigned long long c=1) const;
  /**
    *  Возвращает новую дату, уменьшенную на кол-во дней от текущей
    *
    *  \param [in] c кол-во дней
    */
  Date dec_by_days(unsigned long long c=1) const;
  /**
    *  Обновляет значение даты
    *
    *  \param [in] y число года
    *  \param [in] m число месяца
    *  \param [in] d число дня
    *  \param [in] fmt тип календаря для вх. даты
    */
  bool reset(const Year& y, const Month m, const Day d, const CalendarFormat fmt=Julian);
  /**
   *   Перегруженная версия. Отличается только типом параметров.
   */
  bool reset(const unsigned long long y, const Month m, const Day d, const CalendarFormat fmt=Julian);
  /**
   *  Return string representation of the stored date.<br>
   *  The optional parameter may contain the following format specifiers:<br><ul>
   *    <li>%%% - A literal percent sign (%)
   *    <li>%JY - year number in julian calendar
   *    <li>%GY - year number in grigorian calendar
   *    <li>%MY - year number in milankovic calendar
   *    <li>%Jq - number of month in julian calendar
   *    <li>%Gq - number of month in grigorian calendar
   *    <li>%Mq - number of month in milankovic calendar
   *    <li>%JQ - number of month in julian calendar two digits format
   *    <li>%GQ - number of month in grigorian calendar two digits format
   *    <li>%MQ - number of month in milankovic calendar two digits format
   *    <li>%Jd - day number in julian calendar
   *    <li>%Gd - day number in grigorian calendar
   *    <li>%Md - day number in milankovic calendar
   *    <li>%Jy - last two digits of the year number in julian calendar
   *    <li>%Gy - last two digits of the year number in grigorian calendar
   *    <li>%My - last two digits of the year number in milankovic calendar
   *    <li>%JM - full name of month in julian calendar
   *    <li>%GM - full name of month in grigorian calendar
   *    <li>%MM - full name of month in milankovic calendar
   *    <li>%JF - full name of month in julian calendar (from 1-st face)
   *    <li>%GF - full name of month in grigorian calendar (from 1-st face)
   *    <li>%MF - full name of month in milankovic calendar (from 1-st face)
   *    <li>%Jm - short name of month in julian calendar
   *    <li>%Gm - short name of month in grigorian calendar
   *    <li>%Mm - short name of month in milankovic calendar
   *    <li>%JD - day number in julian calendar in two digits format
   *    <li>%GD - day number in grigorian calendar in two digits format
   *    <li>%MD - day number in milankovic calendar in two digits format
   *    <li>%wd - number of date weekday (sunday=0; monday=1 ...)
   *    <li>%WD - full name of the date weekday
   *    <li>%Wd - short name of date weekday</ul><br>
   *  Each specifier must contain two symbols, except percent.<br>
   *  Unknown format specifiers will be ignored and copied to the output as-is.
   */
  std::string format(std::string fmt = "%JY-%JQ-%JD") const;
};

} // namespace dates_conv
