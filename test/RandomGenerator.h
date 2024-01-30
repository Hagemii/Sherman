#ifndef __RANDOMGENERATOR_H__
#define __RANDOMGENERATOR_H__

// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
#include <atomic>
#include <random>
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------

class MersenneTwister
{
  private:
   static const int NN = 312;
   static const int MM = 156;
   static const uint64_t MATRIX_A = 0xB5026F5AA96619E9ULL;
   static const uint64_t UM = 0xFFFFFFFF80000000ULL;
   static const uint64_t LM = 0x7FFFFFFFULL;
   uint64_t mt[NN];
   int mti;
   void init(uint64_t seed);

  public:
   MersenneTwister(uint64_t seed = 19650218ULL);
   uint64_t rnd();
};

class Xorshift64star
{
  private:
   uint64_t seed;
  public:
   Xorshift64star(uint64_t seed = 19650218ULL);
   uint64_t rnd();
};

// -------------------------------------------------------------------------------------
static thread_local MersenneTwister mt_generator;
static thread_local std::mt19937 random_generator;
static thread_local Xorshift64star fast_generator; 
// -------------------------------------------------------------------------------------

#define always_check(e)                                                               \
    do {                                                                        \
        if (__builtin_expect(!(e), 0)) {                                        \
            std::stringstream ss;                                               \
            ss << __func__ << " in " << __FILE__ << ":" << __LINE__ << '\n';      \
            ss << " msg: " << std::string(#e);                                  \
            throw std::runtime_error(ss.str());                                 \
        }                                                                       \
    } while (0)

#define ENSURE_ENABLED 1
#ifdef ENSURE_ENABLED
#define ensure(e) always_check(e);
#else
#define ensure(e) do {} while(0);
#endif

// -------------------------------------------------------------------------------------
class RandomGenerator
{
  public:
   // ATTENTION: open interval [min, max)
   static uint64_t getRandU64(uint64_t min, uint64_t max)
   {
      uint64_t rand = min + (mt_generator.rnd() % (max - min));
      ensure(rand < max);
      ensure(rand >= min);
      return rand;
   }

   static uint64_t getRandU64Fast(){
      return fast_generator.rnd();
   }
   
   // ATTENTION: open interval [min, max)
   // ATTENTION: power two
   static uint64_t getRandU64PowerTwo(uint64_t maxPowerTwo)
   {
      uint64_t rand = (mt_generator.rnd() & (maxPowerTwo-1));
      return rand;
   }
   static uint64_t getRandU64() { return mt_generator.rnd(); }
   static uint64_t getRandU64STD(uint64_t min, uint64_t max)
   {
      std::uniform_int_distribution<uint64_t> distribution(min, max - 1);
      return distribution(random_generator);
   }

   template <typename T>
   static inline T getRand(T min, T max)
   {
      uint64_t rand = getRandU64(min, max);
      return static_cast<T>(rand);      
   }

   
   static void getRandString(uint8_t* dst, uint64_t size)
   {
      for (uint64_t t_i = 0; t_i < size; t_i++) {
         dst[t_i] = getRand(48, 123);
      }

   }  // namespace utils
};
// -------------------------------------------------------------------------------------


static std::atomic<uint64_t> mt_counter(0);
// -------------------------------------------------------------------------------------
MersenneTwister::MersenneTwister(uint64_t seed) : mti(NN + 1)
{
   static thread_local std::random_device rd;
   init((seed ^ (mt_counter++)) ^ rd());
}
// -------------------------------------------------------------------------------------
void MersenneTwister::init(uint64_t seed)
{
   mt[0] = seed;
   for (mti = 1; mti < NN; mti++)
      mt[mti] = (6364136223846793005ULL * (mt[mti - 1] ^ (mt[mti - 1] >> 62)) + mti);
}
// -------------------------------------------------------------------------------------
uint64_t MersenneTwister::rnd()
{
   int i;
   uint64_t x;
   static uint64_t mag01[2] = {0ULL, MATRIX_A};

   if (mti >= NN)
   { /* generate NN words at one time */

      for (i = 0; i < NN - MM; i++)
      {
         x = (mt[i] & UM) | (mt[i + 1] & LM);
         mt[i] = mt[i + MM] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
      }
      for (; i < NN - 1; i++)
      {
         x = (mt[i] & UM) | (mt[i + 1] & LM);
         mt[i] = mt[i + (MM - NN)] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
      }
      x = (mt[NN - 1] & UM) | (mt[0] & LM);
      mt[NN - 1] = mt[MM - 1] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];

      mti = 0;
   }

   x = mt[mti++];

   x ^= (x >> 29) & 0x5555555555555555ULL;
   x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
   x ^= (x << 37) & 0xFFF7EEE000000000ULL;
   x ^= (x >> 43);

   return x;
}
// -------------------------------------------------------------------------------------
Xorshift64star::Xorshift64star(uint64_t seed_)
{
   static thread_local std::random_device rd;
   seed = (seed_ ^ (mt_counter++)) ^ rd();
}
// -------------------------------------------------------------------------------------
uint64_t Xorshift64star::rnd()
{
   uint64_t x = seed; /* state nicht mit 0 initialisieren */
   x ^= x >> 12;      // a
   x ^= x << 25;      // b
   x ^= x >> 27;      // c
   seed = x;
   return x * 0x2545F4914F6CDD1D;
}

#endif // __RANDOMGENERATOR_H__