#pragma once

#include <QString>
#include <QMap>
#include <cstdint>

namespace MeshCore {

struct RadioConfig {
    QString name;
    uint32_t frequencyKhz;   // Frequency in kHz (protocol expects kHz, not Hz!)
    uint32_t bandwidthHz;    // Bandwidth in Hz
    uint8_t spreadingFactor; // SF 5-12
    uint8_t codingRate;      // CR 5-8

    RadioConfig()
        : frequencyKhz(0), bandwidthHz(0), spreadingFactor(0), codingRate(0) {}

    RadioConfig(const QString& n, uint32_t freq, uint32_t bw, uint8_t sf, uint8_t cr)
        : name(n), frequencyKhz(freq), bandwidthHz(bw), spreadingFactor(sf), codingRate(cr) {}

    bool isValid() const {
        return frequencyKhz >= 300000 && frequencyKhz <= 2500000 &&
               bandwidthHz >= 7800 && bandwidthHz <= 500000 &&
               spreadingFactor >= 5 && spreadingFactor <= 12 &&
               codingRate >= 5 && codingRate <= 8;
    }

    QString toString() const {
        return QString("%1: %2 MHz, BW%3 kHz, SF%4, CR%5")
            .arg(name)
            .arg(frequencyKhz / 1000.0, 0, 'f', 3)
            .arg(bandwidthHz / 1000.0, 0, 'f', 1)
            .arg(spreadingFactor)
            .arg(codingRate);
    }
};

class RadioPresets {
public:
    // Regional presets
    static RadioConfig EU_UK_Narrow() {
        // EU/UK Narrow: 869.618 MHz, BW62.5, SF8, CR8
        // Source: MeshCore October 2025 update - optimized for European ISM band
        return RadioConfig("EU/UK (Narrow)", 869618, 62500, 8, 8);
    }

    static RadioConfig EU_UK_Wide() {
        // EU/UK Wide: 868.0 MHz, BW125, SF11, CR8
        // Legacy wider bandwidth setting
        return RadioConfig("EU/UK (Wide)", 868000, 125000, 11, 8);
    }

    static RadioConfig USA_Canada_Narrow() {
        // USA/Canada Narrow: 910.525 MHz, BW62.5, SF7, CR8
        // Recommended preset - fast transmission, good for urban areas
        return RadioConfig("USA/Canada (Narrow)", 910525, 62500, 7, 8);
    }

    static RadioConfig USA_Canada_Wide() {
        // USA/Canada Wide: 915.0 MHz, BW125, SF11, CR8
        // Legacy wider bandwidth
        return RadioConfig("USA/Canada (Wide)", 915000, 125000, 11, 8);
    }

    static RadioConfig Australia_NZ_Narrow() {
        // Australia/NZ: 915.8 MHz, BW62.5, SF8, CR8
        // Optimized for Australian ISM band
        return RadioConfig("Australia/NZ (Narrow)", 915800, 62500, 8, 8);
    }

    static RadioConfig Asia_433MHz() {
        // Asia 433 MHz band: 433.0 MHz, BW62.5, SF9, CR8
        // Narrow preset for 433MHz ISM band
        return RadioConfig("Asia 433MHz", 433000, 62500, 9, 8);
    }

    static QMap<QString, RadioConfig> getAllPresets() {
        QMap<QString, RadioConfig> presets;
        presets["eu_uk_narrow"] = EU_UK_Narrow();
        presets["eu_uk_wide"] = EU_UK_Wide();
        presets["usa_canada_narrow"] = USA_Canada_Narrow();
        presets["usa_canada_wide"] = USA_Canada_Wide();
        presets["australia_nz_narrow"] = Australia_NZ_Narrow();
        presets["asia_433"] = Asia_433MHz();
        return presets;
    }
};

} // namespace MeshCore
