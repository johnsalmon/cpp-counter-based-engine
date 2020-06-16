# Counter Based Random Numbers in C++ - Discussion and Code

The source code and text files in this directory are intended to
promote discussion and perhaps evolve into a "paper" to be considered
by SG6 (Numerics) subgroup.

The code and text build on:

- "Extension of the C++ random number generators": http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2019/p1932r0.pdf
- "Philox as an extension of the C++ RNG engines": http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2075r0.pdf
- "Vector API for random number generation": http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1068r3.pdf

The proposal is to split the P2075R0's templated philox_engine class
into two parts:

- a new kind of object: a *pseudo random function* (PRF), of which the
new template classes `philox_prf` and `threefry_prf` are specific instances.

- an adaptor class: `counter_based_engine` which is a bona fide Random
Number Engine [rand.req.eng] whose values are obtained by invoking a
pseudo random function.

The key advantage of this formulation is that it clears the way for
ongoing development of new generators.  One can easily imagine the
eventual standardization of counter-based generators based on new and
improved pseudo-random functions (e.g., pseudo random functions based
on established cryptographic primitives like SipHash [ref] or Chacha
[ref]).  It's also allows programs to instantiate their own generators with
features that might be desirable for specific problem domains but that
wouldn't be suitable for standardization (e.g., extremely fast
generators with weak statistical properties, or costly generators with
even stronger statistical properties).

Nothing is lost in the new formulation because a
`counter_based_engine` templated on a Philox PRF satisfies exactly
the same Random Number Engine requirements as P2075R0.

## The Code
Rather than start by writing "standard-ese", it's easier to start with
a concrete example in C++, and then formalize if and when agreement is
reached and details have been worked out.

Source files in this directory constitute a straw-man implementation
of `counter_based_engine`, `philox_prf` and `threefry_prf`:

- details.hpp - defines a few helper functions, concepts, etc.
- counter_base_engine.hpp - defines class counter_based_engine
- philox_prf.hpp    - defines class philox_prf
- threefry_prf.hpp  - defines class threefry_prf
- siphash_prf.hpp, siphash.c - defines class siphash_prf,  which is not intended
    for standardization, but which illustrates how a program could
    instantiate a generator that meets its own needs.
- philoxexample.cpp - a few examples of how one might use the proposed classes
- bench.cpp - demonstrates that prfs can be extremely
  fast and that little or no performance is lost by adapting them
  with counter_based_engine.
- tests.cpp - a few basic sanity and correctness tests.

The code uses C++20 concepts and, in order to get the high bits of the
128-bit product of 64-bit values, uses gcc's uint128_t.  It therefore
currently requires gcc10 to compile, even though nothing in the
proposal is gcc-specific.

The source files are quite short.  The code in philox_prf.hpp is
little more than a succinct implementation of the algorithms described
in P2075R0 and references therein.  There is slightly more code in
threefry_prf.hpp, which demonstrates that the vector API permits the
implementation to use SIMD parallelism.  It should be noted that the
implementation SIMD parallelism in threefry_prf.hpp uses
*non-standard* gcc extensions, but this is an implementation detail
and in no way a requirement.

The code in counter_based_engine.hpp is almost as simple.  It's
basically an implementation of P2075R0, with the philox-specific parts
factored out and an additional seed member and constructor added to
allow more direct program control over the engine.
`counter_based_engine` implements the proposed "vector API" of P1068,
`g(b, e)`, which delivers random values to an output_range.  The
conventional single-value generator, `g()`, is implemented by
calling the vector API member function.

Internally, the `counter_based_engine`'s state consists of
a std::array of input values (which contains the counter) and a
std::array of saved results.  For `philox<n,w>`, the state is 5n/2
scalar values of width w.

With a concrete example in hand, we can begin to enumerate the
requirements.  Eventually, these will have to be expressed in
"standardese", but it's easier to start with plain English.

## Requirements for Pseudo-Random Function (PRF)

A *pseudo-random function* p of type P is a function object that
delivers unsigned integer values to an output_range when called with
an argument that is an input_range of arrays of integers.  In other
words, PRF's are required to support a "vector API" similar to the one
in P1068.  I.e., each invocation of the PRF processes an entire
input_range of inputs, and the resulting random values are written to
an output_range.  This API permits (but does not require) the
underlying implementation to benefit from loop unrolling,
vectorization, SIMD parallelism, etc.  The code in threefry_prf.hpp
shows how a PRF can use SIMD-based parallelism internally.

The vector API allows the program to call a prf's vector API on `Nin`
distinct inputs.  For example:

      prf_t prf;
      static const size_t Nin = ???; // caller decides how many
      // an array of Nin in_types (each of which is an array-type prf_t::in_type)
      prf_t::in_type  in[Nin] = {{...}, {...}, ...};
      uint64_t      out[Nin * prf_t::result_N]; // caller sinks the results
      prf(in, begin(out));
      
which evalutes the underlying psedo-random algorithm on each of the
`Nin` elements of the `in[]` array, with each evaluation writing
`result_N` values into the `out[]` array.

In order to make a PRF usable generically, e.g., by
`counter_based_engine`, information about the characteristics of the
PRF's inputs and outputs must be available to the caller.  There
are many possible ways to represent this information.  One way is via
static constexpr members and nested typedef-names inside the PRF
object (this is the strategy in the example code).  Another
possibility is via a traits class.

In any case, the minimal information that needs to be communicated is:

- the value_type of the PRF's input_range argument.  This is called `P::in_type`
  in the example code.
  
  The in_type must be std::array-like.  That is, it must be subscriptable,
  it must specialize std::tuple_size, it must have a `value_type`
  nested typedef-name, and the `value_type` must be integral.
  `philoxNxW` and `threefryNxW` have `using in_type = array<uintW_least, N/2>`
  and `array<uintW_least, N>` respectively.
        
- the number of meaningful bits in each of the elements of in_type.
  This is called `P::in_bits` in the example code.  `philoxNxW` and
  `threefryNxW` have `in_bits=W`.

- the number of "random" bits in each value delivered to the output range.
  This is called `P::result_bits` in the example code.   `philoxNxW`
  and `threefryNxW` have `result_bits=W`.
  
- the number of random values delivered for each in_type.
  This is called `P::result_N` in the example code.  `philoxNxW`
  and `threefryNxW` have `result_N = N`.

The PRF's operator() member-function itself is declared as follows:

```
    template <ranges::input_range InputRangeOfInTypes, weakly_incrementable O>
    requires ranges::sized_range<InputRangeOfInTypes> &&
             is_same_v<iter_value_t<ranges::iterator_t<InputRangeOfInTypes>>, in_type> &&
             integral<iter_value_t<O>> &&
             indirectly_writable<O, iter_value_t<O>>
    O operator()(InputRangeOfInTypes&& in, O result) const;
```

These constraints (perhaps with some additions) should be part of any
formal specification of a PRF.  This set of constraints guarantees
that:

* the first argument to operator():
    - satisfies `input_range`
    - satisfies `sized_range`
    - has an `iter_value_t` that is_same as `in_type`, i.e., it's `std::array<integral, in_N>`
* and that  the second argument to operator():
    - satisfies `weakly_incrementable`
    - satisfies `indirectly_writable`
    - has an integral `iter_value_t`
   
### Specific PRFs:  philox_prf, threefry_prf, siphash_prf

Three example PRFs are implemented: philox_prf.hpp, threefry_prf.hpp
and siphash_prf.hpp.  The first two (philox and threefry) are widely
used and proposed for standardization.  The last, siphash, illustrates
how a program can create and use an alternative pseudo-random
function.  Siphash (see https://131002.net/siphash/) is widely used as a "message authentication
code" with strong crytographic guarantees that make it potentially interesting as a
random number generator.  By separately specifiying the underlying PRF
and the generic counter_based_engine, programs gain the freedom to
leverage all the other machinery of `<random>` with such alternative
PRFs.  Such alternative PRFs might even, eventually, be candidates for
standardization.


## The counter_based_engine class (CBE)

`counter_based_engine` is a template class declared as:

    template <unsigned_integral ResultType, typename PRF, size_t CounterWords>
    class counter_based_engine;

The unsigned_integral ResultType is similar to the first Uint template
parameter of other standard engines.  It allows the program to
speicify the type of values returned by the generator.  Nevertheless,
the range of result values (min() and max()) is determined by the
PRF's `result_bits`.

The behavior of the counter_based_engine is specified in terms of these
'exposition-only' members:

  - a results member declared as: `array<ResultType, PRF::result_N> results;`
  - an input-value, iv, declared as:  `PRF::in_type iv;`
  - a notional `result_index` (which in practice is stored in results[0])
  - a notional `Y` result_type that communicates from the TA to the GA.

All but the first CounterWords elements of `iv` are set by the
`counter_based_engine`'s constructors and are modified only by the
`seed()` member functions.  The first CounterWords elements of `iv`
are managed by the `counter_based_engine` itself.  They represent a
'counter' with values in the range
0,...,2^(CounterWords*PRF::in_bits).

Given a type P that satisfies the requirements of a PRF, and an unsigned
integral ResultType, and an unsigned value CounterWords (typically 1 or 2)

    counter_base_engine<ResultType, P, CounterWords>

satisfies the requirements of a Random Number Engine:

- the result_type is `ResultType`

- Equality comparison and stream insertion and extraction operators compare,
insert and extract the state's `iv` and `result_index`.

- constructors and seed methods:

  In the straw-man implementation (this could easily be changed), all
  the standard constructors and seed methods forward through
  seed(SeedSeq), where the SeedSeq is a std::seed_seq constructed from
  the constructor's or seed() method's arguments.

  To seed a CBE from a seed_seq, the `seed_seq`'s `generate` method is called once
  to generate enough bits to populate all but the
  first CounterWords elements of the CBE's `iv`.  Then the first CounterWords
  elements of `iv` and `rindex` are set to 0.

- The Transition Algorithm is:
```
   if result_index is zero,
       prf(ranges::single_view(iv), begin(results))
       increment the counter {iv[0] ... iv[CounterWords-1]}
   Y = results[result_index]
   result_index = (result_index+1) modulo result_N
```

- The Generation Algorithm is:

```
   return the value of Y was assigned in the last TA
```

### Vector API for generation of random numbers

The TA and GA allow (but do not require) `counter_based_engine`'s
vector API (see P1068R3) to first send any stored `results` to the
output range, and then to call the underlying PRF's vector API with
output delivered "directly" to the vector generator's output range
argument.  Furthermore, the underlying PRF's input argument is also a
range, and can be generated "lazily" with an `iota_view` and a
`transform_view`.  Thus, no additional temporary storage or memory
copying is required to benefit from any optimization that may be
present in the underlying PRF's vector implementation.  

The straw-man implementation in counter_based_engine.hpp and
threefry_prf.hpp demonstrates these aspects of the API.  Benchmarks
and examination of generated assembly show that the engine's vector
API offers a significant performance advantage.  For example
`counter_based_engine<uint64_t, threefry4x64_prf, 1>`'s vector API
produces 1024 random 64-bit values at approximately 5.4GB/s on a
single 3.7 GHz Skylake core (using AVX-512 instructions).

### General properties of counter_based_engine:

An instance of `counter_based_engine<ResultType, PRF, CounterWords>`
allows for

    2^(in_bits*(in_N-CounterWords))

*different, statistically independent* random sequences, each of length `PRF::result_N*2^(CounterWords*in_bits)`

A counter_based_engine's size is: `sizeof(PRF::in_type) + PRF::result_N*sizeof(ResultType)`.  For example:

- `counter_based_engine<uint64_t, philox4x64, 1>` defines 2^320 different random
sequences of length 2^66.  The size is 10 64-bit integers.

The CounterWords template parameter allows long-period generators, even with
PRFs that are inherently 32-bit.  For example:

- `counter_based_engine<uint32_t, philox2x32, 1>` defines 2^64 different random
sequences of length *only* 2^33.  The size is 5 32-bit integers.  These sequences
might be considered too short for some applications.

but 

- `counter_based_engine<uint32_t, philox2x32, 2>` defines 2^32 different random
sequences of length 2^65.  The size is 5 32-bit integers.  These sequences
are long enough for any practical application (especially when one considers
that there are 2^32 of them).


### Extensions to the required Uniform Random Number Engine API:

Unfortunately, the standard Uniform Random Number Engine API doesn't
offer a natural way to specify or manage a large number of different
sequences.  The "SeedSeq" interface is both painfully slow and
appears to be prone to collisions (e.g., knuth_b(seed_seq({13122}))
and knuth_b(seed_seq({28597})).

This proposal therefore specifies additional constructor and seed
member functions that give the program more direct control over the
invocation of the underlying PRF:

- a typedef-name:  prf_type - the type of the underlying PRF.

- A constructor and corresponding `seed` member function that take
  an `input_range` (or `initializer list`) argument.   The values
  in the input range are copied directly into all but the first
  CounterWords elements of the engine's internal `iv` array.

  For example,

      using eng_t = counter_based_engine<philox4x64_prf>
      uint64_t a = ..., b= ..., c = ..., d = ..., e = ...;   // 2^320 distinct engines
      eng_t eng1({a,b,c,d,e});  // initializer_list
   
      eng_t eng2; // default-constructed
      array<uint64_t, 5> abcde = {a, b, c, d, e};
      eng2.seed(abcde);        // range
      
      assert(eng1 == eng2);

  When using these members, the program gains direct control the
  values stored in the iv and passed to the PRF.  By taking such
  control, the program takes responsibility for avoiding undesirable
  collisions.

