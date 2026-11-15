#pragma once
// Provisiones que el científico puede comprar en el hangar (pestaña MOCHILA) y que el
// astronauta usa durante el viaje (incidents.cpp). Los ids se guardan en la partida.
#include <algorithm>
#include <string>
#include <vector>

struct BackpackItemDef {
    const char* id;
    const char* nameEs;
    const char* nameEn;
    const char* descEs;
    const char* descEn;
    float costMillions;
};

inline const std::vector<BackpackItemDef>& BackpackCatalog() {
    static const std::vector<BackpackItemDef> items = {
        { "tool",   "Caja de herramientas", "Toolkit",        "Llaves, destornilladores y alicates.",          "Wrenches, screwdrivers and pliers.", 45.0f },
        { "patch",  "Kit de parches",       "Patch kit",      "Parches adhesivos para el casco de la nave.",   "Sticky patches for the ship's hull.", 40.0f },
        { "water",  "Reserva de agua",      "Water reserve",  "Depósito extra de agua potable.",               "An extra tank of drinking water.", 45.0f },
        { "oxygen", "Tanque de oxígeno",    "Oxygen tank",    "Botella de oxígeno de repuesto.",               "A spare bottle of oxygen.", 55.0f },
        { "fuel",   "Combustible extra",    "Extra fuel",     "Depósito auxiliar para maniobras.",             "An auxiliary tank for manoeuvres.", 60.0f },
        { "tape",   "Cinta americana",      "Duct tape",      "Un rollo de cinta adhesiva muy resistente.",    "A roll of very strong sticky tape.", 15.0f },
        { "gum",    "Chicle",               "Chewing gum",    "Un paquete de chicles de menta.",               "A pack of mint chewing gum.", 8.0f },
        { "bags",   "Bolsas de plástico",   "Plastic bags",   "Bolsas con cierre hermético.",                  "Bags with an airtight seal.", 10.0f },
        { "towel",  "Toalla",               "Towel",          "Una toalla grande y absorbente.",               "A big, absorbent towel.", 10.0f },
        { "manual", "Manual de vuelo",      "Flight manual",  "El libro con todos los procedimientos.",        "The book with every procedure.", 20.0f }
    };
    return items;
}

inline const BackpackItemDef* FindBackpackItem(const std::string& id) {
    for (const auto& it : BackpackCatalog()) if (id == it.id) return &it;
    return nullptr;
}

inline bool BackpackHas(const std::vector<std::string>& pack, const std::string& id) {
    return std::find(pack.begin(), pack.end(), id) != pack.end();
}

inline float BackpackCost(const std::vector<std::string>& pack) {
    float total = 0.0f;
    for (const auto& id : pack) if (const BackpackItemDef* d = FindBackpackItem(id)) total += d->costMillions;
    return total;
}
