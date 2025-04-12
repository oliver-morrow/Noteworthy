

#include <unordered_map>
#include "Symbol.hpp"

#ifdef NOTEWORTHY_QT
const std::unordered_map<Symbol::SymbolType, QString> Symbol::symbolSvgPaths {
    {Symbol::CAPACITOR, ":/png/png/capacitor.svg"},
    {Symbol::RESISTOR, ":/png/png/resistor.svg"},
    {Symbol::INDUCTOR, ":/png/png/inductor.svg"},
    {Symbol::BATTERY, ":/png/png/battery.svg"},
    {Symbol::DIODE, ":/png/png/diode.svg"},
    {Symbol::SWITCH, ":/png/png/switch.svg"},
    {Symbol::AC_SOURCE, ":/png/png/ac_source.svg"},
    {Symbol::DC_SOURCE, ":/png/png/dc_source.svg"},
    {Symbol::CURRENT_SOURCE, ":/png/png/current_source.svg"}
};
#endif
