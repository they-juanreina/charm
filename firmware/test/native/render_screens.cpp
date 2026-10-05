// Dibuja cada pantalla del charm con el código real de src/Display.cpp y guarda el lienzo tal cual
// (.bmp1, 3904 bytes). Sirve para el manual y para comparar con el diseño de Paper sin la placa.
//
//   tools/render_screens.sh    compila esto y convierte las capturas a PNG
#include "Display.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static std::string outDir;
static Display display;

static void save(const char *name)
{
    std::ofstream f(outDir + "/" + name + ".bmp1", std::ios::binary);
    f.write((const char *)display.buffer(), BITMAP_BYTES);
    std::cout << name << "\n";
}

static std::string readFile(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        std::cerr << "uso: render_screens <salida> <poema.txt> <imagen.bmp1>\n";
        return 1;
    }
    outDir = argv[1];
    display.begin();
    const std::string name = DEFAULT_NAME;

    // Reverso: la carta del rango de la baraja (los mismos argumentos que main.cpp)
    display.showBack(nullptr, name, 0, 17, 4100);
    save("reverso-as");
    display.showBack(nullptr, name, 6, 17, 4100);
    save("reverso-6");
    display.showBack(nullptr, name, 14, 17, 4100);
    save("reverso-14");
    display.showBack(nullptr, name, 3, 17, 3450);
    save("reverso-bateria-baja");
    display.showBack(nullptr, name, 31, 40, 4100);
    save("reverso-31");

    // Poema real de la baraja, todas sus páginas
    TextLayout::Layout L;
    display.layoutCard(readFile(argv[2]), L);
    for (uint8_t p = 0; p < L.pageCount(); p++) {
        display.showPage(L, p);
        save(("poema-p" + std::to_string(p + 1)).c_str());
    }

    // Dedicatoria de ejemplo: la carta de as se dibuja como un poema
    if (argc > 4) {
        TextLayout::Layout D;
        display.layoutCard(readFile(argv[4]), D);
        display.showPage(D, 0);
        save("dedicatoria");
    }

    std::string img = readFile(argv[3]);
    if (img.size() == BITMAP_BYTES) {
        display.showImage((const uint8_t *)img.data());
        save("imagen");
    }

    display.showWifi(AP_NAME, AP_PASSWORD, "http://192.168.4.1");
    save("wifi");
    display.showWifiConnected(name, 17);
    save("conectado");
    display.showCharging(name, 4050, true);
    save("cargando");
    display.showCharging(name, 4200, true);
    save("carga-completa");
    display.showNotice("Sin cartas", "Mantén el botón 2", "para abrir el WiFi", "y subir poemas.");
    save("aviso-sin-cartas");
    display.showNotice("Poca carga", "Medida: 3.15 V.", "Carga el charm antes", "de usar el WiFi.");
    save("aviso-bateria-baja");
    return 0;
}
