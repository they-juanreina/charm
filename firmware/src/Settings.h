#pragma once
// Ajustes del usuario guardados en NVS (nombre que aparece en el reverso de la carta).
#include <string>

namespace Settings
{
void begin();
std::string name();                    // UTF-8, por defecto DEFAULT_NAME
bool setName(const std::string &utf8); // recorta, quita saltos de línea, máx. NAME_MAX_BYTES
} // namespace Settings
