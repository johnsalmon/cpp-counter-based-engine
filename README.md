# Counter Based Random Numbers in C++ - Discussion and Code

The source code and text files in this directory are intended to
promote discussion and perhaps evolve into a "paper" to be considered
by SG6 (Numerics) subgroup.

The code and text build on:

- "Extension of the C++ random number generators": http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2019/p1932r0.pdf
- "Philox as an extension of the C++ RNG engines": http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2075r0.pdf

The proposal is to split the P2075R0's templated philox_engine class
into two parts:

- a new kind of object: a *keyed pseudo random function* (KPRF),
of which the template class philox_keyed_prf is a specific instance.

- an adaptor class: counter_based_engine which is a bona fide Random
Number Engine [rand.req.eng] whose values are obtained by invoking a
keyed pseudo random function.

The key advantages of this formulation are:

1. it naturally generalizes to allow the creation of counter-based
engines from other pseudo-random functions, e.g., Threefry, SHA-1,
AES, ChaCha.
1. constructing a counter_based_generator is fast enough to be
practical in a tight loop.  Such engines can be created and destroyed
"on-the-fly" and employed with standard "random number distributions"
in a one-engine-per-object strategy.  Such a strategy is well-suited
to multi-threaded and parallel programs, but is impractical with
conventional engines due to space and time overheads.
1. it allows programs to generate random values directly from the
underlying pseudo-random function.

Nothing is lost in the new formulation because a
'counter_based_engine' templated on a Philox KPRF satisfies exactly
the same Random Number Engine requirements as P2075R0.

## The Code
Rather than start by writing "standard-ese", it's easier to start with
a concrete example in C++, and then formalize if and when agreement is
reached and details have been worked out.

Source files in this directory constitute a straw-man implementation
of both counter_based_engine and philox_keyed_prf:

- details.hpp - defines a few helper functions, concepts, etc.
- counter_base_engine.hpp - defines class counter_based_engine
- philox_keyed_prf.hpp    - defines class philox_keyed_prf
- philoxexample.cpp - a few examples of how one might use the proposed classes
- philoxbench.cpp - demonstrates that philox_keyed_prf is extremely
  fast and that little or no performance is lost by adapting it
  with counter_based_engine.

The code uses C++20 concepts and, in order to get the high bits of the
128-bit product of 64-bit values, uses gcc's uint128_t.  It therefore
currently requires gcc10 to compile, even though nothing in the
proposal is gcc-specific.

The source files are quite short.  The code in philox_keyed_prf.hpp is
little more than some boilerplate to define the values and types that
would be required of any KPRF, and a succinct implementation of the
Philox algorithm described in P2075R0 and references therein.

The code in counter_based_engine.hpp is almost as simple.  It's
basically an implementation of P2075R0, with the philox-specific parts
factored out and additional members and constructors added to allow
more direct program control over the engine.

Internally, the counter_based_engine's state consists of an instance
of the KPRF, an input array (which contains the counter) and a
std::array of saved results.  For philox<n,w>, the state is 5n/2
scalar values of width w.

With a concrete example in hand, we can begin to enumerate the
requirements.  Eventually, these will have to be expressed in
"standardese", but it's easier to start with plain English.

## Requirements for Keyed Pseudo-Random Function (KPRF)

- There are static constexpr unsigned members called:
  - key_bits, key_N
  - in_bits, in_N
  - result_bits, result_N

  whose values are the number of significant bits in each "word" and
  the number of "words" in the KPRF's key, its input and its result.

- There are public unsigned integral types called:
  - key_value_type
  - in_value_type
  - result_value_type

  that are able to represent values up to at least 2^(xx_bits)-1

- There is a public result_type:

      using result_type = array<result_value_type, result_N>;

- There is a constructor that takes an input_range (or
  initializer_list) argument, and a corresponding 'rekey' method that
  also takes an input_range (or initializer_list) argument.  (Note
  that 'input_range' is the C++20 concept std::input_range).  The
  input_range (or initalizer_list) must have an integral
  range_value_t.

  These methods initialize key_N values, k_0 .. k_Nm1, by reading from
  the input range, kr, as if by:

       auto p = ranges::begin(kr);
       auto e = ranges::end(kr);
       k_0 = (p!=e) ? (*p++)&key_bit_mask : 0;
       k_1 = (p!=e) ? (*p++)&key_bit_mask : 0;
       ...
       k_Nm1 = (p!=e) ? (*p++)&key_bit_mask : 0;

  where key_bit_mask = 2^key_bits - 1

  The keys are then transformed by the KPRF's initialization algorithm (IA)
  into some internal state.

  N.B.  For philox, the IA simply copies k0 through k_Nm1 into the
  "exposition only" internal state of the KPRF.  For other KPRFs
  (e.g., AES), the initialization algorithm may be more complex.

- There is an operator() that takes an input_range argument and
  returns a result_type.  It works as follows:

  Exactly in_N values, in_i are initialized from the input_range
  argument, ir as if by:

      auto p = ranges::begin(ir);
      auto e = ranges::end(ir);
      in_0 = (p!=e) ? (*p++)&in_bit_mask : 0;
      in_1 = (p!=e) ? (*p++)&in_bit_mask : 0;
      ...
      in_Nm1 = (p!=e) ? (*p++)&in_bit_mask : 0;

  where in_bit_mask is 2^in_bits -1.

  Then the KPRF's mixing algorithm is applied to the internal state
  and in_0 .. in_Nm1, and the resulting out_N values are returned in a
  std::array 'result_type'.

  The "mixing algorithm" (MA) must be described in the KPRF's
  specification.

  Ideally, the mixing algorithm produces values that are "randomized",
  by a change in any bit of either the input or the key.  

  N.B. - it is notoriously hard to state precisely what is meant by
  "random".  The existing standard fails to define it with any
  precision.  Nevertheless, it is important to emphasize that the
  values produced by KPRF{key}(input) should be statistically
  independent when even a single bit of the key or a single bit of the
  input is changed.  On the other hand, KPRFs are not intended for
  cryptography.  It is not required that it be "hard" to reconstruct
  the key from known inputs/result pairs, for example.

  The KPRF's operator() must be a pure function.  I.e.  any two KPRFs
  that compare equal must return the same value when invoked with the
  same inputs.  If two KPRFs k1 and k2 compare equal at some time,
  then they must continue to compare equal at least until one of their
  non-const member function is called.  The invocation operator() is a
  const member function.

- There is a default constructor and a no-argument rekey() method
  that is equivalent to calling:

      rekey(ranges::views::empty<unsigned>);

  I.e., it (re)initializes the KPRF with zero-valued k_i.

- There are comparison operators == and !=.  KPRFs that compare equal
  return the same results for the same inputs.  KPRFs that compare
  unequal return statistically independent, "random" results whether
  called with the same or different inputs.

- There are stream insertion and  extraction operators << and >>
  that save and restore the internal state of a KPRF.

The requirements are all straightforward to implement.  The only
"interesting" components of a KPRF are the mixing algorithm and the
initialization algorithm.  And in fact, the initialization algorithm
is often trivial as well.

## The counter_based_engine class (CBE)

The counter_based_engine is also fairly simple.  Most of the code in
counter_based_engine.hpp is there to satisfy the requirements of a
Random Number Engine.

A counter_based_engine is a template class declared as:

    template <typename KPRF>
    class counter_based_engine;

The behavior of the counter_based_engine is specified in terms of these
'exposition-only' members:

  - an instance of the KPRF, kprf
  - an iv member declared as:  array<in_value_type, in_N> iv
  - a results member declared as: KPRF::result_type results;
  - a notional counter (which in practice is stored in the high bits of iv[in_N-1])
  - a notional 'rindex' (which in practice is stored in results[0])
  - a notional 'counter_overflow' bool (which in practice is also stored in results[0])
  - a notional 'Y' result_type that communicates from the TA to the GA.


Given a type K that satisfies the requirements of a KPRF,

    counter_base_engine<K>

### counter_based_engine satisfies the requirements of a Random Number Engine:

- the result_type is K::result_type

- Equality comparison and stream insertion and extraction operators compare,
insert and extract each of the components of the state, respectively.

- The Transition Algorithm is:
```
   if counter_overflow is set
       do nothing, return
   if rindex is zero,
       copy iv into an array, in, suitable as input to the KPRF.
       in[in_N-1] = counter
       results = kprf(in)
       counter = (counter + 1) modulo 2^in_bits
   Y = results[rindex]
   rindex = (rindex+1) modulo result_N
   if rindex is zero and counter is zero,
      set counter_overflow
```

- The Generation Algorithm is:

```
   if Y was assigned in the last TA, then return it,
   otherwise, throw an out_of_range exception
```

- constructors and seed methods:

  In the straw-man implementation (this could easily be changed), all
  the standard constructors and seed methods forward through
  seed(SeedSeq), where the SeedSeq is a std::seed_seq constructed from
  the constructor's or seed() method's arguments.

  To seed a CBE from a seed_seq, the seed_seq.generate is called once
  to generate enough bits to populate the KPRF's key and all but the
  last element of the CBE's iv.  The counter and rindex are set to 0
  and counter_overflow is cleared.

### General properties of counter_based_engine:

A counter_based_engine allows for

    2^(key_bits*key_N + in_bits*(in_N-1))

*different* random sequences, each of length result_N*2^in_bits

A counter_based_engine's size is: in_N + result_N + sizeof(KPRF)

For philox4x64, there are 2^320 different random
sequences of length 2^66.  The size is 10 64-bit integers.

For philox2x32, there are 2^64 different random
sequences of length 2^33.  The size is 5 32-bit integers.

### Extensions to the required Uniform Random Number Engine API:

Unfortunately, the standard Uniform Random Number Engine API doesn't
offer a natural way to specify or manage a large number of different
sequences.  The "SeedSeq" interface is both painfully slow and
appears to be prone to collisions (e.g., knuth_b(seed_seq({13122}))
and knuth_b(seed_seq({28597})).

This proposal therefore specifies additional constructor and seed
member functions that give the programmer more direct control over
the keys and values that are passed to the underlying KPRF.

All existing standard random number engines specify non-required
public members, e.g., mersenne_twister_engine::tempering_u.  The
counter_based_engine specifies more than most, but is not breaking new
ground by doing so.

In addition to the types and methods required by [rand.req.eng],
counter_based_engine has the following members:

- kprf_type - the type of the underlying KPRF.
- getkprf() - a member function that returns a copy of the current kprf

- types and constexpr values inherited from KPRF:
  - in_value_type, key_value_type, result_value_type
  - in_bits, key_bits, result_bits,
  - in_N, key_N, result_N

- A constructor and corresponding seed member function that take as
  arguments two input_ranges (or initializer lists).  The first is
  passed to the kprf's constructor and a second is used to initialize
  the CBE's 'iv' member.  An out_of_range exception is thrown if
  ranges::size(in) is greater than or equal to in_N-1.

  It can be used (with initializer_lists) like:

      using eng_t = counter_based_engine<philox4x64_kprf>
      uint64_t a = ..., b= ...;   // 2^128 possible distinct values
      uint64_t c = ..., d = ..., e = ...;  // 2^192 possible distinct values
      eng_t eng({a,b}, {c,d,e}); // 2^320 distinct engines
   
  The programmer gains direct control over the key and iv values, but
  is also responsibile for avoiding undesirable collisions.

- An analogous constructor/seed member takes a KPRF as its first argument.
  The counter_based_engine's internal kprf is initialized by
  copy-construction.  E.g.,

      using kprf_t = philox4x64_kprf;
      kprf_t::key_value_type a = ..., b = ...;
      kprf_t kprf({a, b});     // 2^128 possible distinct kprf's
      ...
      kprf_t::in_value_type c = ..., d=..., e=...; // 2^192 different distinct values
      auto eng = counter_based_engine(prf, {c, d, e}); // 2^320 distinct engines
