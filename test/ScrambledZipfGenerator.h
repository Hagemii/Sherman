#ifndef __SCRAMBLEDZIPF_H__
#define __SCRAMBLEDZIPF_H__


#include "FNVHash.h"

// #include "ZipfGenerator.hpp"
#include "ZipfRejectionInversion.h"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

class ScrambledZipfGenerator
{
  public:
   uint64_t min, max, n;
   double theta;
   std::random_device rd;
   std::mt19937 gen;
   zipf_distribution<> zipf_generator;
   // 10000000000ul
   // [min, max)
   ScrambledZipfGenerator(uint64_t min, uint64_t max, double theta) : min(min), max(max), n(max - min), gen(rd()), zipf_generator((max - min) * 2, theta) {
   }
   uint64_t rand();
   uint64_t rand(uint64_t offset);
};

// -------------------------------------------------------------------------------------
uint64_t ScrambledZipfGenerator::rand()
{
   uint64_t zipf_value = zipf_generator(gen);
   return min + (FNV::hash(zipf_value) % n);
}
// -------------------------------------------------------------------------------------
uint64_t ScrambledZipfGenerator::rand(uint64_t offset)
{
   uint64_t zipf_value = zipf_generator(gen);
   return (min + ((FNV::hash(zipf_value + offset)) % n));
}

// class ScrambledZipfGenerator
// {
//   public:
//    uint64_t min, max, n;
//    double theta;
//    ZipfGenerator zipf_generator;
//    // 10000000000ul
//    // [min, max)
//    ScrambledZipfGenerator(uint64_t min, uint64_t max, double theta) : min(min), max(max), n(max - min), zipf_generator((max - min) * 2, theta) {
//    }
//    uint64_t rand();
//    uint64_t rand(uint64_t offset);
// };
// -------------------------------------------------------------------------------------

#endif // __SCRAMBLEDZIPF_H__