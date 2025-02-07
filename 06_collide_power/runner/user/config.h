#pragma once

#include "../module/interface.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

// number of samples
constexpr size_t NUMBER_SAMPLES = 300000;

// repeat per randomly choosen parameter, counteract dynamic states
constexpr size_t EXPERIMENT_REPEAT = 1000;

struct [[gnu::packed]] row_t {
    time_t        time;
    char          exp[20];
    uint16_t      erep;
    uint16_t      dur;
    float         la;
    uint8_t       value[16 * 12];
    uint8_t       guess[16 * 12];
    measurement_t measurements;
};
static_assert(sizeof(time_t) == 8);
static_assert(sizeof(row_t {}.guess) == 192);
static_assert(sizeof(row_t {}.value) == 192);

std::vector<std::string> row_t_fields {
    "('time', 'u8')", //
    "('Exp', 'S20')", //
    "('ERep', 'u2')", //
    "('Dur', 'u2')",  //
    "('la', 'f4')",   //

    "('Value', 'S192')", //
    "('Guess', 'S192')", //

    "('Energy', 'u4')",     //
    "('EnergyPP0', 'u4')",  //
    "('EnergyDRAM', 'u4')", //
    "('Ticks', 'u8')",      //
    "('Volt', 'u8')",       //
    "('PState', 'u8')",     //
    "('Temp', 'u8')",       //
    "('APerf', 'u8')",      //
    "('Mperf', 'u8')",      //
    "('dreg', 'i8')",       //
    "('inst_a', 'u8')",     //
    "('inst_b', 'u8')",     //
    "('temp_a', 'u8')",     //
    "('temp_b', 'u8')",     //
};
