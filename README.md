# Counter Based Random Numbers in C++ - Discussion and Code

The source code and text files in this directory are intended to
promote discussion and perhaps evolve into a "paper" to be considered
by SG6 (Numerics) subgroup.

The code and text build on:

- "Extension of the C++ random number generators": http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2019/p1932r0.pdf
- "Philox as an extension of the C++ RNG engines": http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2075r0.pdf

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
- bench.cpp - demonstrates that the prfs is extremely
  fast and that little or no performance is lost by adapting them
  with counter_based_engine.
- tests.cpp - a few basic sanity and correctness tests.

The code uses C++20 concepts and, in order to get the high bits of the
128-bit product of 64-bit values, uses gcc's uint128_t.  It therefore
currently requires gcc10 to compile, even though nothing in the
proposal is gcc-specific.

The source files are quite short.  The code in philox_prf.hpp and
threefry_prf.hpp are little more than a succinct implementation of the
algorithms described in P2075R0 and references therein.

The code in counter_based_engine.hpp is almost as simple.  It's
basically an implementation of P2075R0, with the philox-specific parts
factored out and an additional seed member and constructor added to
allow more direct program control over the engine.

Internally, the `counter_based_engine`'s state consists of
a std::array of input values (which contains the counter) and a
std::array of saved results.  For `philox<n,w>`, the state is 5n/2
scalar values of width w.

With a concrete example in hand, we can begin to enumerate the
requirements.  Eventually, these will have to be expressed in
"standardese", but it's easier to start with plain English.

## Requirements for Pseudo-Random Function (PRF)

A *pseudo-random function* p of type P is a function object that returns
an array of unsigned integer values when called with an argument that
is a std::range or initializer_list integral values.

Let il be an initializer_list of integral values and let ir be an
input_range of integral values.  Then these expressions are valid:

     p(ir)
     p(il)

and return arrays of unsigned integers.

In order to make a PRF usable generically, e.g., by
counter_based_engine, some information about the characteristics of
the function-object's inputs and outputs needs to be available to the
caller It's not clear how best to represent this information.  One
possibility is via static constexpr members and nested typedef-names
inside the PRF object (this is the strategy in the example code).
Another possibility is via a traits class.

In any case, the minimal information that needs to be communicated is:

- the number of meaningful bits in each of the values of the input range.
  This is called `P::in_bits` in the example code.

- the number of values consumed from the input range or initializer list.
  This is called `P::in_N` in the example code.
  
- the number of "random" bits in each integral element of the returned array.
  This is called `P::result_bits` in the example code.
  
- the type of the returned array.  This is called `P::result_type`
  and is:

        array<P::result_value_type, P::result_N> 
  in the example code.
   
### Specific PRFs:  philox_prf, threefry_prf, siphash_prf

Three example PRFs are implemented: philox_prf.hpp, threefry_prf.hpp
and siphash_prf.hpp.  The first two (philox and threefry) are widely
used and proposed for standardization.  The last, siphash, illustrates
how a program can create and use an alternative pseudo-random
function.  Siphash (see https://131002.net/siphash/) is widely used as a "message authentication
code" with strong guarantees that make it potentially interesting as a
random number generator.  By separately specifiying the underlying PRF
and the generic counter_based_engine, programs gain the freedom to
leverage all the other machinery of <random> with such alternative
PRFs.  Such alternative PRFs might even, eventually, be candidates for
standardization.


## The counter_based_engine class (CBE)

A counter_based_engine is a template class declared as:

    template <typename PRF>
    class counter_based_engine;

The behavior of the counter_based_engine is specified in terms of these
'exposition-only' members:

  - a results member declared as: `PRF::result_type results;`
  - a input-value (iv) declared as:  `array<integer_type, PRF::in_N> iv;`
       where integer_type has at least `PRF::in_bits` bits.
  - a notional 'result_index' (which in practice is stored in results[0])
  - a notional 'Y' result_type that communicates from the TA to the GA.

All but the last (`iv.back()`) elements of `iv` are set by the
`counter_based_engine`'s constructors and are modified only by the
`seed()` member functions.  The last element (`iv.back()`) element is
managed by the `counter_based_engine` itself.

Given a type P that satisfies the requirements of a PRF,

    counter_base_engine<P>

### counter_based_engine satisfies the requirements of a Random Number Engine:

- the result_type is `P::result_type`

- Equality comparison and stream insertion and extraction operators compare,
insert and extract the state's `iv` and `result_index`.

- The Transition Algorithm is:
```
   if result_index is zero,
       results = prf(iv)
       iv.back() = (iv.back() + 1) modulo 2^in_bits
   Y = results[result_index]
   result_index = (result_index+1) modulo result_N
```

- The Generation Algorithm is:

```
   return the value of Y was assigned in the last TA
```

Note that example implementation in counter_based_engine.hpp contains
additional logic that throws an `out_of_range` exception if the counter
"wraps" module 2^in_bits.  This prevents inadvertent re-use of the
same sequence.  As far as I can tell, this is permissible for a Random
Number Engine, and I believe the complexity/usabiliy tradeoff is a net win.
Nevertheless, it's easily removed.

- constructors and seed methods:

  In the straw-man implementation (this could easily be changed), all
  the standard constructors and seed methods forward through
  seed(SeedSeq), where the SeedSeq is a std::seed_seq constructed from
  the constructor's or seed() method's arguments.

  To seed a CBE from a seed_seq, the `seed_seq`'s `generate` method is called once
  to generate enough bits to populate all but the
  last element of the CBE's iv.  Then `iv.back()` and `rindex` are set to 0.

### General properties of counter_based_engine:

A counter_based_engine allows for

    2^(in_bits*(in_N-1))

*different, statistically independent* random sequences, each of length `PRF::result_N*2^in_bits`

A counter_based_engine's size is: `PRF::in_N + PRF::result_N` integers

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
member functions that give the program more direct control over the
invocation of the underlying PRF:

- a typedef-name:  prf_type - the type of the underlying PRF.

- A constructor and corresponding `seed` member function that take
  an `input_range` (or `initializer list`) argument.   The values
  in the input range are copied directly into all but the last
  elements of the engine's internal `iv` array.

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

