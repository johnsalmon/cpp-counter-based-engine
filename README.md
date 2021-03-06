# Counter Based Random Numbers in C++ - Discussion and Code

The source code and text files in this directory are intended to
promote discussion.  They have evolved into a "paper" to be considered
by SG6 (Numerics) subgroup.  It should be available in late July 2020
as http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2075r0.pdf

The code and text build on:

- "Extension of the C++ random number generators": http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2019/p1932r0.pdf
- "Philox as an extension of the C++ RNG engines": http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2075r0.pdf
- "Vector API for random number generation": http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1068r3.pdf

P2075R1 proposes two strategies for standardizing the philox family of
random number generators.  The first is a straightforward development
of P2075R0.  The second (elaborated here) splits P2075R0's
templated philox_engine class into two parts:

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

## Differences from P2075R1

The code here differs slightly from the API in P2075R1.  Specific differences
are:
- it distinguishes betwen input_ and output_characteristics
  of the prf.  I.e., prf::input_word_size, prf::output_word_size, prf::input_value_type
  and prf::output_value_type instead of prf::word_size, prf::result_type.
  
- there are no max() and  min() methods in the prfs.  They're only in
  counter_based_engine.

- the round-count template parameter r is consistently declared with type size_t.

- the prf::operator() returns an output_iterator that "points" one past the
  last written output value.
  
- prfs have a "bulk generation" method called 'generate'
  which invokes the underlying function multiple times.  The
  implementation in threefry_prf demonstrates how such a
  method can exploit simd capabilities.
  
- counter_based_engine is extended with a "bulk generation" method
  following P1068r3 that calls its pseudo-random function's generate
  method.
  
- counter_based_engine::seed(result_type value) considers only the lowest
  prf::input_word_size bits of value.
  
- in philox_prf, when X and K are initialized from the input, X is
  initialized first, followed by K.  It's worth noting that the "known
  answer test" vectors in P2075R1 are consistent with this order, and
  are inconsistent with the order specified elsewhere in P2075R1.

- counter_based_engine has seed methods and constructors that allow
  the caller to specify all elements of the internal "key".  The
  number of such elements is counter_based_engine::seed_count, with
  counter_based_engine::seed_word_size bits.
  

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
delivers unsigned integer values to an output iterator when called with
an argument that is an input iterator to an array of integers.  This is
essentially the API proposed in P2075R1.

In order to make a PRF usable generically, e.g., by
`counter_based_engine`, information about the characteristics of the
PRF's inputs and outputs must be available to the caller.  There are
many possible ways to represent this information.  One way is via
static constexpr members and nested typedef-names inside the PRF
object (this is the strategy in the example code and in P2075R1).
Another possibility is via a traits class.

In any case, the minimal information that needs to be communicated is:

- the value_type of the PRF's input_iterator argument.  This is called
  `P::input_value_type` in the example code.
  
- the number of input values that will be consumed by each invocation
  of the PRF's operator().  This is called `P::input_count` in the
  example code.  `philoxNxW` has `input_count=3N/2`.  `threefryNxW`
  has `input_count=2N`.

- the number of meaningful bits in each of the values of the input_iterator.
  This is called `P::input_word_size` in the example code.  `philoxNxW` and
  `threefryNxW` have `input_word_size=W`.

- the type of value delivered to the output range.  This is called
  `P::output_value_type` in the example code.

- the number of "random" bits in each value delivered to the output range.
  This is called `P::output_word_size` in the example code.   `philoxNxW`
  and `threefryNxW` have `result_bits=W`.
  
- the number of random values delivered for each in_type.
  This is called `P::output_count` in the example code.  `philoxNxW`
  and `threefryNxW` have `output_count = N`.

Note that P2075R1 does not distinguish between `input_word_size`
and `output_word_size`.  It proposes only one value, called `word_size`,
which is sufficient for philox, but slightly less general.  Also note
that P2075R1 does not distinguish between `input_value_type` and
`output_value_type`.  It proposes only `result_type`, which is
sufficient for philox, but slightly less general.

### Support for Vector API
In addtion, the code here provides a "vector API":  A 'generate'
method that takes a range of ranges and calls the underlying
function on each sub-range, generating integers that are delivered
to the output range.  This API permits (but does not require) the
underlying implementation to benefit from loop unrolling,
vectorization, SIMD parallelism, etc.  The code in threefry_prf.hpp
shows how a PRF can use SIMD-based parallelism internally.
The code in counter_based_engine.hpp makes use of the generate()
method in its implementation of the proposed (P1068R3) vector
API for random number engines.

The vector API allows the program to call a prf's vector API on `Nin`
distinct inputs.  For example:

      prf_t prf;
      static const size_t Nin = ???; // caller decides how many
      // an array of Nin in_types (each of which is an array-type prf_t::in_type)
      arrray<prf_t::input_value_type, prf_t::input_count> in[Nin] = {{...}, {...}, ...};
      uint64_t      out[Nin * prf_t::output_count]; // caller sinks the results
      prf.generate(in|views::transform([](auto& a){return begin(a);}), begin(out));
      
which evalutes the underlying psedo-random algorithm on each of the
`Nin` elements of the `in[]` array, with each evaluation writing
`output_count` values into the `out[]` array.

The PRF's generate() member-function itself is declared as follows:

```
    template <ranges::input_range InRange, weakly_incrementable O>
    requires ranges::sized_range<InRange> &&
             integral<iter_value_t<ranges::range_value_t<InRange>>> &&
             integral<iter_value_t<O>> &&
             indirectly_writable<O, iter_value_t<O>>
    O generate(InRange&& inrange, O result) const;
```

These constraints (perhaps with some additions) should be part of any
formal specification of a PRF.  This set of constraints guarantees
that:

* the first argument to generate(in, out):
    - satisfies `input_range`
    - satisfies `sized_range`
    - has an `iter_value_t` that is an `input_iterator` with an integral value_type.
* and that  the second argument to generate(in, out):
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

    template <typename PRF, size_t CounterWords>
    class counter_based_engine;

The TA, GA and other properties and characteristics are described in P2075R1.

Given a type P that satisfies the requirements of a PRF, and an unsigned
integral ResultType, and an unsigned value CounterWords (typically 1 or 2)

    counter_base_engine<PRF, c>

satisfies the requirements of a Random Number Engine:

- the result_type is `PRF::result_type`

- Equality comparison and stream insertion and extraction operators compare,
insert and extract the state's `iv` and `result_index`.

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

An instance of `counter_based_engine<PRF, c>`
allows for

    2^(in_bits*(in_count-c))

*different, statistically independent* random sequences, each of length `PRF::output_count*2^(c*input_word_size)`

A counter_based_engine's size is: `PRF::input_count*sizeof(PRF::input_value_type) + PRF::output_count*sizeof(result_type)`.  For example:

- `counter_based_engine<philox4x64, 1>` defines 2^320 different random
sequences of length 2^66.  The size is 10 64-bit integers.

The CounterWords template parameter allows long-period generators, even with
PRFs that are inherently 32-bit.  For example:

- `counter_based_engine<philox2x32, 1>` defines 2^64 different random
sequences of length *only* 2^33.  The size is 5 32-bit integers.  These sequences
might be considered too short for some applications.

but 

- `counter_based_engine<philox2x32, 2>` defines 2^32 different random
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

