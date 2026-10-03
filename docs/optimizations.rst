Under the hood
==============

This page explains how CDFpp reads, writes and converts time fast: what each part costs,
what was changed, and why it works. It is for advanced users and contributors who want to
understand the numbers on :doc:`performance`, or to change the code without losing them.

Every claim here was measured. The numbers come from an AMD Ryzen 7 5800X (Zen 3, AVX2, no
AVX-512, 32 MB L3), Linux 7.2, GCC 16, and a btrfs file system on an NVMe SSD, unless a
section says otherwise. Your numbers will differ. The reasons should not.

.. raw:: html

    <svg width="0" height="0" style="position:absolute" aria-hidden="true">
      <defs>
        <marker id="cdfpp-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7"
                markerHeight="7" orient="auto-start-reverse">
          <path d="M 0 0 L 10 5 L 0 10 z" style="fill: var(--color-foreground-secondary)"/>
        </marker>
      </defs>
    </svg>

How we work
-----------

Three habits explain most of what follows.

1. **Find the floor first.** Before optimizing a step, measure the cheapest thing that could
   possibly do the same job, like one ``memcpy`` or one ``write()`` of the same bytes. If
   the step is already close, stop. If it is far, the gap is the work to do.
2. **Measure real files.** Synthetic benchmarks missed every big problem described below.
   They came from real CDAWeb files: checksums, 4-D column-major records, hundreds of
   variables.
3. **Explain with counters, not guesses.** ``perf record`` says where the time goes.
   ``perf stat`` with the CPU's own event counters says why: cache misses, a full store
   queue, mispredicted branches. Each section quotes the counters that decided it.

Every change also had to keep the output identical. 64 real files, saved before and after
every change of this page, give the same bytes. The test suite runs under AddressSanitizer,
UndefinedBehaviorSanitizer and ThreadSanitizer. And each new test is checked against a
deliberately broken version of the code, to prove it can fail.

The floor
---------

Saving an 83 MB file whose values are already in the file's layout:

.. list-table::
   :header-rows: 1
   :widths: 50 25 25

   * - What
     - To memory
     - To a file
   * - The cheapest possible: one ``memcpy`` / one ``write()``
     - 9.3 ms
     - 9.1 ms
   * - ``cdf::io::save`` / ``pycdfpp.save``
     - 10.6 ms
     - 9.6 ms

The rest is the kernel. In ``perf``, a save to memory is 46% ``memcpy`` and 40% the kernel
zeroing the fresh pages it hands out (Fedora kernels zero every new page,
``init_on_alloc=1``). A save to a file is the kernel copying into its page cache, and zeroing
that cache's new pages. Nothing in CDFpp is left to remove there.

So the work below is about everything that is *not* that case: other byte orders, column
major files, checksums, files with much metadata, and files that already exist.

Writing a file
--------------

Saving happens in two steps. First, CDFpp lays out every record of the file: it computes each
record's size and offset, without writing anything. Then it writes the records in file order,
to a memory buffer or to a file.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 350" role="img" aria-label="The save pipeline">
      <rect class="panel" x="10" y="40" width="150" height="70" rx="8"/>
      <text class="tb c" x="85" y="70">CDF in memory</text>
      <text class="ts c" x="85" y="90">variables, attributes</text>
      <path class="arrow" d="M160 75 H196"/>
      <rect class="blue" x="200" y="30" width="170" height="90" rx="8" stroke-width="1.5"/>
      <text class="text-blue c" x="285" y="58">1 · Lay out</text>
      <text class="t c" x="285" y="80">size and offset of</text>
      <text class="t c" x="285" y="98">every record, first</text>
      <path class="arrow" d="M370 75 H406"/>
      <rect class="orange" x="410" y="30" width="170" height="90" rx="8" stroke-width="1.5"/>
      <text class="text-orange c" x="495" y="58">2 · Write</text>
      <text class="t c" x="495" y="80">records in file order,</text>
      <text class="t c" x="495" y="98">values in file layout</text>
      <path class="arrow" d="M580 62 L616 48"/>
      <path class="arrow" d="M580 90 L616 106"/>
      <rect class="green" x="620" y="18" width="130" height="54" rx="8" stroke-width="1.5"/>
      <text class="text-green c" x="685" y="40">memory buffer</text>
      <text class="ts c" x="685" y="58">allocated once</text>
      <rect class="green" x="620" y="80" width="130" height="54" rx="8" stroke-width="1.5"/>
      <text class="text-green c" x="685" y="102">file</text>
      <text class="ts c" x="685" y="120">written over in place</text>
      <path class="line dash" d="M495 120 V158"/>
      <rect class="panel" x="200" y="160" width="550" height="180" rx="8"/>
      <text class="tb" x="215" y="184">How the values reach the writer</text>
      <rect class="green" x="215" y="198" width="190" height="26" rx="13"/>
      <text class="t c" x="310" y="216">same layout as memory</text>
      <text class="t" x="420" y="216">one copy, straight from your array</text>
      <rect class="blue" x="215" y="232" width="190" height="26" rx="13"/>
      <text class="t c" x="310" y="250">other byte order</text>
      <text class="t" x="420" y="250">swapped while copied</text>
      <rect class="orange" x="215" y="266" width="190" height="26" rx="13"/>
      <text class="t c" x="310" y="284">column major, 2-D+ records</text>
      <text class="t" x="420" y="284">transposed in small tiles</text>
      <rect class="violet" x="215" y="300" width="190" height="26" rx="13"/>
      <text class="t c" x="310" y="318">MD5 checksum</text>
      <text class="t" x="420" y="318">hashed on a 2nd thread, while written</text>
    </svg>
    <figcaption>Saving: lay out everything, then write it once, in order.</figcaption>
    </figure>

Laying out first has a big advantage: the file's exact size is known before the first byte
is written. The next sections use it.

One allocation, at the exact size
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``save()`` without a path returns the file in memory. It used to reserve the size of the
values plus 64 KB for the metadata. Files with much metadata outgrew that:

1. THEMIS ESA files have 383 variables, each with its own records and attributes.
2. Their metadata takes more than 64 KB.
3. Near the end of the save, while writing attribute entries, the buffer was full.
4. The whole file, 56 MB, was copied into a buffer twice as big. Its fresh pages were zeroed
   by the kernel.

The layout step gives the exact size. The buffer is now reserved at that size, once. A test
checks that the buffer's capacity equals its size, with and without compression and
checksum.

.. list-table::
   :header-rows: 1

   * - File
     - Before
     - After
   * - THEMIS ESA, 383 variables, 58 MB
     - 18.7 ms
     - 10.8 ms
   * - THEMIS FGM, 64 variables, 56 MB
     - 17.5 ms
     - 10.1 ms
   * - MMS MEC, 98 variables, 3.5 MB
     - 0.55 ms
     - 0.22 ms

Another byte order: swapped while copied
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Many CDAWeb files store their values big-endian ("network" encoding), while x86 and ARM
computers are little-endian. Saving such a file means swapping the bytes of every value on
the way out. CDFpp never swaps your arrays: it converts a chunk at a time, as it writes.

That used to cost two passes and a fresh buffer for every 1 MB chunk. Now the values are
swapped while they are copied, into one buffer reused for the whole variable. When saving to
memory, they are even swapped straight into the file's memory, with no buffer at all.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 250" role="img" aria-label="Swapping byte order, before and after">
      <text class="tb" x="10" y="22">Before</text>
      <rect class="blue" x="10" y="36" width="120" height="44" rx="6"/>
      <text class="t c" x="70" y="63">your values</text>
      <path class="arrow" d="M130 58 H186"/>
      <text class="ts c" x="158" y="50">memcpy</text>
      <rect class="red" x="190" y="36" width="150" height="44" rx="6"/>
      <text class="t c" x="265" y="55">new 1 MB chunk</text>
      <text class="ts c" x="265" y="71">pages zeroed by the kernel</text>
      <path class="arrow" d="M340 58 H396"/>
      <text class="ts c" x="368" y="50">swap</text>
      <rect class="orange" x="400" y="36" width="150" height="44" rx="6"/>
      <text class="t c" x="475" y="55">same chunk,</text>
      <text class="ts c" x="475" y="71">read and written again</text>
      <path class="arrow" d="M550 58 H606"/>
      <text class="ts c" x="578" y="50">copy</text>
      <rect class="green" x="610" y="36" width="140" height="44" rx="6"/>
      <text class="t c" x="680" y="63">file or buffer</text>
      <text class="tm" x="10" y="104">Each chunk: a fresh allocation, then the values cross memory three times.</text>
      <line class="faint" x1="10" y1="122" x2="750" y2="122"/>
      <text class="tb" x="10" y="150">After</text>
      <rect class="blue" x="10" y="164" width="120" height="44" rx="6"/>
      <text class="t c" x="70" y="191">your values</text>
      <path class="arrow" d="M130 176 L226 170"/>
      <text class="ts c" x="178" y="154">swap while</text>
      <text class="ts c" x="178" y="166">copying</text>
      <rect class="orange" x="230" y="148" width="150" height="40" rx="6"/>
      <text class="t c" x="305" y="166">one 2 MB chunk</text>
      <text class="ts c" x="305" y="181">reused for the variable</text>
      <path class="arrow" d="M380 168 H606"/>
      <text class="ts c" x="493" y="160">write()</text>
      <rect class="green" x="610" y="148" width="140" height="40" rx="6"/>
      <text class="t c" x="680" y="173">file</text>
      <path class="arrow" d="M130 198 L606 214"/>
      <text class="ts c" x="370" y="226">saving to memory: swapped straight into the file's bytes</text>
      <rect class="green" x="610" y="196" width="140" height="40" rx="6"/>
      <text class="t c" x="680" y="221">memory buffer</text>
    </svg>
    <figcaption>Big-endian values: one pass instead of three, and no allocation per chunk.</figcaption>
    </figure>

Saving to memory writes straight into the file's bytes, at whatever offset each value
lands. Values there are not aligned: each record starts after a 16-byte header. So the swap
reads and writes each value through ``memcpy``, which the compiler turns into plain unaligned
moves. Reading them through a typed pointer would be undefined behaviour, and
UndefinedBehaviorSanitizer would say so.

The byte swap itself compiles to a loop of scalar ``bswap`` instructions: the baseline
x86-64 instruction set has no byte shuffle (``pshufb`` came with SSSE3). That is fine. One
``bswap`` per cycle is far faster than memory.

83 MB of big-endian values: 16.9 → 12.0 ms to memory, 16.8 → 12.5 ms to a file.

The size of each ``write()``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When writing to a file, the chunk size matters more than expected. btrfs pays about 13 µs
for every ``write()`` call, whatever its size: it locks and reserves space per call. Saving
the same 83 MB big-endian file:

.. list-table::
   :header-rows: 1

   * - Chunk
     - 32 KB
     - 128 KB
     - 512 KB
     - 1 MB
     - 2 MB
     - 8 MB
     - 16 MB
   * - Save
     - 47.4 ms
     - 23.8 ms
     - 15.1 ms
     - 13.4 ms
     - **12.6 ms**
     - 12.5 ms
     - 16.1 ms

Small chunks drown in calls. Very big chunks no longer fit in the CPU caches between the swap
and the copy into the kernel. CDFpp uses 2 MB, one huge page.

Saving over an existing file
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This one was the biggest surprise: saving over an existing file took four times longer than
saving a new one.

1. Opening a file with ``O_TRUNC`` empties it.
2. btrfs and ext4 recognise "emptied, then written again". It is how many programs replace a
   file, and a crash in the middle used to leave empty files behind.
3. To prevent that, they write the file to disk as soon as it is closed (btrfs:
   ``BTRFS_INODE_FLUSH_ON_CLOSE``; ext4: ``auto_da_alloc``). Renaming a new file over an
   old one does the same.
4. So ``close()`` waited for the disk, and, on a compressed btrfs, for the compression.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 200" role="img" aria-label="Timeline of saving over an existing file">
      <text class="tb" x="10" y="24">Saving 80 MB over an existing file</text>
      <text class="t r" x="150" y="68">O_TRUNC</text>
      <rect class="blue" x="160" y="50" width="155" height="28" rx="4"/>
      <text class="t c" x="237" y="69">write 10.3 ms</text>
      <rect class="red" x="315" y="50" width="273" height="28" rx="4"/>
      <text class="text-red c" x="451" y="69">close(): flush + compress, 18.2 ms</text>
      <text class="t r" x="150" y="118">in place</text>
      <rect class="blue" x="160" y="100" width="111" height="28" rx="4"/>
      <text class="t c" x="215" y="119">write 7.4 ms</text>
      <rect class="green" x="271" y="100" width="6" height="28"/>
      <text class="ts" x="284" y="119">cut to size, close: 0 ms</text>
      <line class="faint" x1="160" y1="150" x2="610" y2="150"/>
      <g class="ts">
        <text class="ts c" x="160" y="168">0</text>
        <text class="ts c" x="235" y="168">5</text>
        <text class="ts c" x="310" y="168">10</text>
        <text class="ts c" x="385" y="168">15</text>
        <text class="ts c" x="460" y="168">20</text>
        <text class="ts c" x="535" y="168">25</text>
        <text class="ts c" x="610" y="168">30 ms</text>
      </g>
      <text class="tm" x="160" y="192">The flush only moves disk work earlier, into your save. Done later, it costs you nothing.</text>
    </svg>
    <figcaption>Writing over the file in place, then cutting it to its new size, skips the flush on close.</figcaption>
    </figure>

CDFpp now opens an existing file without emptying it, writes over it, and cuts what is left
past the new end. A test checks that saving a small file over a bigger one leaves none of the
old bytes.

83 MB saved over an existing file: 40.1 → 7.9 ms.

What a crash does is unchanged in practice: a crash in the middle of a save leaves a broken
file either way. To be sure a file reached the disk, you need ``fsync``, which CDFpp does
not call today (see `issue #127 <https://github.com/SciQLop/CDFpp/issues/127>`_ for the
planned option, and how it can overlap the disk with the CPU).

File system tricks that did not help
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Writing 80 MB, the plain ``write()`` of a new file took 9.5 ms. These were slower or no
better:

.. list-table::
   :header-rows: 1
   :widths: 35 15 50

   * - Trick
     - 80 MB
     - Why
   * - ``fallocate`` first
     - 9.6 ms
     - The kernel allocates space quickly anyway.
   * - 4 threads, ``pwrite`` at 4 offsets
     - 10.2 ms
     - The file's lock serializes them.
   * - ``mmap`` the file, then ``memcpy``
     - 18.9 ms
     - One page fault per 4 KB page, each mapping a page cache page.
   * - ``O_DIRECT``
     - no gain
     - On a compressed btrfs, it falls back to normal writes.
   * - Writing to ``/tmp`` (tmpfs)
     - 29 ms
     - Slower than the SSD's page cache on this machine, even for a raw ``write()``. Not something CDFpp controls.

Column-major files: transposing in tiles
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A CDF file stores multi-dimensional records in one of two orders. Row major (C order) puts
the last index fastest, like numpy. Column major (Fortran order) puts the first index fastest.
CDFpp always gives you row-major arrays. So loading a column-major file transposes every
record, and saving one transposes it back.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 190" role="img" aria-label="Row major and column major order">
      <text class="tb" x="20" y="22">One record of 2 × 3 values</text>
      <g>
        <rect class="blue" x="20" y="40" width="40" height="34"/><text class="t c" x="40" y="62">a</text>
        <rect class="blue" x="60" y="40" width="40" height="34"/><text class="t c" x="80" y="62">b</text>
        <rect class="blue" x="100" y="40" width="40" height="34"/><text class="t c" x="120" y="62">c</text>
        <rect class="orange" x="20" y="74" width="40" height="34"/><text class="t c" x="40" y="96">d</text>
        <rect class="orange" x="60" y="74" width="40" height="34"/><text class="t c" x="80" y="96">e</text>
        <rect class="orange" x="100" y="74" width="40" height="34"/><text class="t c" x="120" y="96">f</text>
      </g>
      <text class="tb" x="200" y="22">In memory</text>
      <text class="t r" x="330" y="62">row major</text>
      <g>
        <rect class="blue" x="340" y="44" width="34" height="28"/><text class="t c" x="357" y="63">a</text>
        <rect class="blue" x="374" y="44" width="34" height="28"/><text class="t c" x="391" y="63">b</text>
        <rect class="blue" x="408" y="44" width="34" height="28"/><text class="t c" x="425" y="63">c</text>
        <rect class="orange" x="442" y="44" width="34" height="28"/><text class="t c" x="459" y="63">d</text>
        <rect class="orange" x="476" y="44" width="34" height="28"/><text class="t c" x="493" y="63">e</text>
        <rect class="orange" x="510" y="44" width="34" height="28"/><text class="t c" x="527" y="63">f</text>
      </g>
      <text class="t r" x="330" y="104">column major</text>
      <g>
        <rect class="blue" x="340" y="86" width="34" height="28"/><text class="t c" x="357" y="105">a</text>
        <rect class="orange" x="374" y="86" width="34" height="28"/><text class="t c" x="391" y="105">d</text>
        <rect class="blue" x="408" y="86" width="34" height="28"/><text class="t c" x="425" y="105">b</text>
        <rect class="orange" x="442" y="86" width="34" height="28"/><text class="t c" x="459" y="105">e</text>
        <rect class="blue" x="476" y="86" width="34" height="28"/><text class="t c" x="493" y="105">c</text>
        <rect class="orange" x="510" y="86" width="34" height="28"/><text class="t c" x="527" y="105">f</text>
      </g>
      <text class="tb" x="580" y="22">A 1-D record</text>
      <g>
        <rect class="green" x="580" y="64" width="34" height="28"/><text class="t c" x="597" y="83">x</text>
        <rect class="green" x="614" y="64" width="34" height="28"/><text class="t c" x="631" y="83">y</text>
        <rect class="green" x="648" y="64" width="34" height="28"/><text class="t c" x="665" y="83">z</text>
      </g>
      <text class="ts" x="580" y="112">the same in both orders:</text>
      <text class="ts" x="580" y="126">nothing to transpose</text>
      <text class="tm" x="20" y="160">Most CDAWeb variables have 1-D records (a vector, a spectrum): they are written straight</text>
      <text class="tm" x="20" y="178">from memory. MMS FPI distributions are 32 × 16 × 32 per record: those need the transpose.</text>
    </svg>
    <figcaption>Majority only matters for records of two dimensions or more.</figcaption>
    </figure>

How it was done
^^^^^^^^^^^^^^^

The old code built a table with, for every value of a record, where to read it and where to
write it: 16 bytes of indexes per value, 4 times more than a float. It then copied the values
one by one, each to a scattered place, into a temporary record, and copied that back.

The CPU's counters told the story. Per value: 6 cycles, 2.6 L1 cache refills, and, for 48% of
the time, the core could not dispatch any instruction because its **store queue was full**.
Every store went to a different cache line, so stores could not merge, and the queue waited
on the cache.

How it is done
^^^^^^^^^^^^^^

Three changes, each guided by the counters.

1. **No table.** The record is walked with strides, like nested loops. The innermost loop
   follows the output's fastest dimension. An odometer steps the others.
2. **Tiles.** Values move in tiles: all the output's fastest dimension, but only 32 bytes of
   the input's contiguous dimension. Only a few cache lines wait for stores at any time.
3. **Blocks in registers.** Inside a tile, values move in 4 × 4 blocks (for floats). The block
   function has fixed sizes, so GCC transposes it in vector registers, with plain SSE2:
   4 loads, 8 shuffles, 4 stores. No intrinsics, so it works on every x86-64 CPU, and the
   compiler is free to do better on others.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 290" role="img" aria-label="A tile and a 4 by 4 block">
      <text class="tb" x="20" y="22">One record, as the input stores it</text>
      <g class="faint">
        <rect x="20" y="40" width="256" height="128" style="fill: var(--d-panel)"/>
        <path d="M36 40 V168 M52 40 V168 M68 40 V168 M84 40 V168 M100 40 V168 M116 40 V168 M132 40 V168 M148 40 V168 M164 40 V168 M180 40 V168 M196 40 V168 M212 40 V168 M228 40 V168 M244 40 V168 M260 40 V168"/>
        <path d="M20 56 H276 M20 72 H276 M20 88 H276 M20 104 H276 M20 120 H276 M20 136 H276 M20 152 H276"/>
      </g>
      <rect class="orange" x="20" y="40" width="128" height="128" style="fill-opacity: 0.55" stroke-width="2"/>
      <rect x="84" y="40" width="64" height="64" class="stroke-blue" stroke-width="3"/>
      <text class="ts" x="20" y="186">contiguous in the input →</text>
      <text class="text-orange" x="20" y="206">tile: 32 bytes wide</text>
      <text class="text-blue" x="160" y="206">4 × 4 block</text>
      <path class="arrow" d="M150 72 C 220 72, 260 90, 318 92"/>
      <text class="tb" x="330" y="22">In registers</text>
      <g>
        <text class="ts r" x="352" y="60">load</text>
        <rect class="blue" x="360" y="44" width="120" height="22" rx="3"/><text class="tmono c" x="420" y="60">a0 a1 a2 a3</text>
        <rect class="blue" x="360" y="70" width="120" height="22" rx="3"/><text class="tmono c" x="420" y="86">b0 b1 b2 b3</text>
        <rect class="blue" x="360" y="96" width="120" height="22" rx="3"/><text class="tmono c" x="420" y="112">c0 c1 c2 c3</text>
        <rect class="blue" x="360" y="122" width="120" height="22" rx="3"/><text class="tmono c" x="420" y="138">d0 d1 d2 d3</text>
      </g>
      <path class="arrow" d="M486 94 H554"/>
      <text class="ts c" x="520" y="86">8 shufps</text>
      <g>
        <rect class="green" x="560" y="44" width="120" height="22" rx="3"/><text class="tmono c" x="620" y="60">a0 b0 c0 d0</text>
        <rect class="green" x="560" y="70" width="120" height="22" rx="3"/><text class="tmono c" x="620" y="86">a1 b1 c1 d1</text>
        <rect class="green" x="560" y="96" width="120" height="22" rx="3"/><text class="tmono c" x="620" y="112">a2 b2 c2 d2</text>
        <rect class="green" x="560" y="122" width="120" height="22" rx="3"/><text class="tmono c" x="620" y="138">a3 b3 c3 d3</text>
        <text class="ts" x="688" y="98">store</text>
      </g>
      <text class="tm" x="330" y="186">Each load reads 4 values next to each other in the input.</text>
      <text class="tm" x="330" y="206">Each store writes 4 values next to each other in the output.</text>
      <line class="faint" x1="20" y1="226" x2="740" y2="226"/>
      <text class="tm" x="20" y="252">The tile is walked block by block, then the next tile. A 32-byte tile keeps about 8 output lines</text>
      <text class="tm" x="20" y="272">waiting for stores, and the input lines it reads stay in L1. Wider tiles evict them.</text>
    </svg>
    <figcaption>Strided walk, 32-byte tiles, and 4 × 4 blocks the compiler transposes in SSE2 registers.</figcaption>
    </figure>

What GCC makes of the 4 × 4 float block, at ``-O3`` for baseline x86-64:

.. code-block:: nasm

    movups  (%rdi), %xmm0          ; 4 rows in
    movups  (%rdi,%rsi), %xmm5
    ...
    shufps  $136, %xmm5, %xmm0     ; 8 shuffles
    shufps  $221, %xmm5, %xmm2
    ...
    movups  %xmm4, (%rdx)          ; 4 columns out
    movups  %xmm4, (%rdx,%rax)

The counters for one 2 MB chunk of FPI-shaped records (32 × 16 × 32 floats), transposed in
cache:

.. list-table::
   :header-rows: 1

   * -
     - Cycles per value
     - Instructions per value
     - L1 refills per value
     - Store queue full
   * - Index table (before)
     - 6.0
     - 9.4
     - 2.6
     - 48% of cycles
   * - Strided, 64-byte tiles
     - 2.3
     - 7.5
     - 0.42
     - 14%
   * - Strided, 32-byte tiles
     - 2.0
     - 8.6
     - 0.40
     - 4%
   * - 32-byte tiles of 4 × 4 blocks (now)
     - **1.36**
     - 3.6
     - 0.40
     - 5%
   * - ``memcpy``, for scale
     - 0.35
     - 0.4
     - –
     - –

Branch mispredictions, TLB misses and address-generation stalls were negligible in every
version. With 128-byte tiles, L1 refills and miss-buffer allocations double, and the
transpose gets slower again.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 210" role="img" aria-label="Transpose speed, GB per second">
      <text class="tb" x="20" y="22">Transposing 2 MB of 32 × 16 × 32 float records, GB/s (higher is better)</text>
      <text class="t r" x="230" y="56">index table (before)</text>
      <rect class="red" x="240" y="42" width="49" height="22" rx="3"/>
      <text class="t" x="296" y="58">3.3</text>
      <text class="t r" x="230" y="90">strided, 64-byte tiles</text>
      <rect class="orange" x="240" y="76" width="138" height="22" rx="3"/>
      <text class="t" x="385" y="92">9.2</text>
      <text class="t r" x="230" y="124">strided, 32-byte tiles</text>
      <rect class="orange" x="240" y="110" width="147" height="22" rx="3"/>
      <text class="t" x="394" y="126">9.8</text>
      <text class="t r" x="230" y="158">+ 4 × 4 SSE2 blocks (now)</text>
      <rect class="green" x="240" y="144" width="218" height="22" rx="3"/>
      <text class="text-green" x="465" y="160">14.5</text>
      <text class="t r" x="230" y="192">hand-written AVX2 8 × 8</text>
      <rect class="blue" x="240" y="178" width="269" height="22" rx="3" style="fill-opacity:0.6"/>
      <text class="t" x="516" y="194">17.9 — not used, see below</text>
    </svg>
    <figcaption>Each step removed what the counters showed: the store queue, then L1 misses, then instructions.</figcaption>
    </figure>

The same function serves both directions, so loading column-major files got faster too.
Saving also skips the temporary record: values are transposed straight from your array into
the output chunk.

.. list-table::
   :header-rows: 1

   * - MMS FPI distribution file (column major)
     - Before
     - After
   * - Save, burst, 158 MB
     - 75 ms
     - 32 ms
   * - Save, fast survey, 210 MB
     - 96 ms
     - 43 ms
   * - Load, burst (gzip included)
     - 81 ms
     - 58 ms
   * - Load, fast survey (gzip included)
     - 130 ms
     - 97 ms

Why not AVX2 or AVX-512?
^^^^^^^^^^^^^^^^^^^^^^^^

We tried. Compiled for AVX2, GCC builds its own 8 × 8 block from 48 ``vpermd``: slow
shuffles that cross the two halves of the register. It is no faster than SSE2. A hand-written
8 × 8 transpose with AVX2 intrinsics (8 ``unpack``, 8 ``shuffle``, 8 ``permute2f128``) is 22%
faster on the transpose alone.

But the transpose is only a third of a real save. End to end, that is about 8%, and only for
column-major files with multi-dimensional records. It would need its own run-time dispatch
(see `Choosing the instruction set at run time`_), one kernel per value size, and an
AVX-512 version that the test machine cannot run. Not worth it, for now.

Checksums: MD5 at full speed, for free
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Many recent files end with an MD5 checksum of the whole file: MMS, Solar Orbiter, RBSP. CDFpp
keeps a file's checksum when you load and save it again. On those files, MD5 was 70 to 90%
of the save time. MD5 can't be split across threads: each 64-byte block needs the result of
the one before. Two things were left to do: make one MD5 faster, and hide everything else
behind it.

A shorter critical path
^^^^^^^^^^^^^^^^^^^^^^^

MD5 runs 64 steps per block. Each step computes a new value ``b`` from the four state words,
and the next step needs it at once. So MD5's speed is the length of this chain, not the
number of instructions: it ran at 1.8 instructions per cycle, on a CPU that can do 4 or more.

The trick is to keep the newest value, ``b``, out of as many operations as possible
(`animetosho/md5-optimisation <https://github.com/animetosho/md5-optimisation>`_):

1. Add the constant and the message word to ``a`` first. ``a`` is three steps old, so this
   happens while waiting for ``b``.
2. In rounds 17 to 32, the function ``G = (b & d) | (c & ~d)`` picks bits from ``b`` or
   ``c``. Its two halves have no bit in common, so ``|`` can be ``+``, and the half without
   ``b`` can be added early too.
3. In rounds 33 to 48, write ``H = b ^ (c ^ d)``: ``c ^ d`` is ready before ``b``.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 250" role="img" aria-label="One MD5 step, critical path before and after">
      <text class="tb" x="20" y="22">One step of round 2: the chain from b to the next b</text>
      <text class="t r" x="110" y="68">before</text>
      <rect class="red" x="120" y="50" width="100" height="28" rx="4"/><text class="t c" x="170" y="69">b ^ c</text>
      <path class="arrow" d="M220 64 H236"/>
      <rect class="red" x="240" y="50" width="80" height="28" rx="4"/><text class="t c" x="280" y="69">&amp; d</text>
      <path class="arrow" d="M320 64 H336"/>
      <rect class="red" x="340" y="50" width="80" height="28" rx="4"/><text class="t c" x="380" y="69">^ c</text>
      <path class="arrow" d="M420 64 H436"/>
      <rect class="red" x="440" y="50" width="70" height="28" rx="4"/><text class="t c" x="475" y="69">+ a</text>
      <path class="arrow" d="M510 64 H526"/>
      <rect class="red" x="530" y="50" width="70" height="28" rx="4"/><text class="t c" x="565" y="69">+ K</text>
      <path class="arrow" d="M600 64 H616"/>
      <rect class="red" x="620" y="50" width="56" height="28" rx="4"/><text class="t c" x="648" y="69">+ M</text>
      <text class="ts" x="684" y="62">then rotl,</text>
      <text class="ts" x="684" y="76">+ b</text>
      <text class="t r" x="110" y="148">after</text>
      <rect class="green" x="120" y="104" width="300" height="26" rx="4"/>
      <text class="t c" x="270" y="122">a + K + M + (c &amp; ~d)  — while b is computed</text>
      <path class="arrow" d="M420 117 C 450 117, 450 140, 466 140"/>
      <rect class="red" x="120" y="130" width="100" height="28" rx="4"/><text class="t c" x="170" y="149">b &amp; d</text>
      <path class="arrow" d="M220 144 H466"/>
      <rect class="red" x="470" y="130" width="70" height="28" rx="4"/><text class="t c" x="505" y="149">+</text>
      <text class="ts" x="548" y="142">then rotl,</text>
      <text class="ts" x="548" y="156">+ b</text>
      <line class="faint" x1="20" y1="180" x2="740" y2="180"/>
      <rect class="red" x="20" y="196" width="18" height="14" rx="2"/>
      <text class="t" x="46" y="208">on the critical path: waits for b</text>
      <rect class="green" x="300" y="196" width="18" height="14" rx="2"/>
      <text class="t" x="326" y="208">off the path: runs in parallel</text>
      <text class="tm" x="20" y="236">6 dependent operations become 2, before the rotation and the final addition.</text>
    </svg>
    <figcaption>Reordering the additions takes most of the work off the chain. Same digests, checked against RFC 1321.</figcaption>
    </figure>

MD5 went from 878 to 969 MB/s. A BMI1 trick on the fourth round function (``andn``)
saves an instruction but not a step of the chain, so it was left out.

Hashing while writing
^^^^^^^^^^^^^^^^^^^^^

The checksum used to be computed, then the block written, one after the other. Now every
block of 1 MB or more is hashed on a second thread while the calling thread writes it. Both
finish before the next block starts, so the data stays valid without any copy. In
WebAssembly builds without threads, the two simply run one after the other.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 170" role="img" aria-label="Hashing and writing, before and after">
      <text class="t r" x="100" y="46">before</text>
      <rect class="violet" x="110" y="30" width="160" height="26" rx="3"/><text class="t c" x="190" y="48">MD5 block 1</text>
      <rect class="blue" x="270" y="30" width="40" height="26" rx="3"/><text class="ts c" x="290" y="47">write</text>
      <rect class="violet" x="310" y="30" width="160" height="26" rx="3"/><text class="t c" x="390" y="48">MD5 block 2</text>
      <rect class="blue" x="470" y="30" width="40" height="26" rx="3"/><text class="ts c" x="490" y="47">write</text>
      <rect class="violet" x="510" y="30" width="160" height="26" rx="3"/><text class="t c" x="590" y="48">MD5 block 3</text>
      <rect class="blue" x="670" y="30" width="40" height="26" rx="3"/><text class="ts c" x="690" y="47">write</text>
      <text class="t r" x="100" y="102">after</text>
      <text class="ts r" x="100" y="118">2 threads</text>
      <rect class="violet" x="110" y="86" width="160" height="26" rx="3"/><text class="t c" x="190" y="104">MD5 block 1</text>
      <rect class="violet" x="270" y="86" width="160" height="26" rx="3"/><text class="t c" x="350" y="104">MD5 block 2</text>
      <rect class="violet" x="430" y="86" width="160" height="26" rx="3"/><text class="t c" x="510" y="104">MD5 block 3</text>
      <rect class="blue" x="110" y="116" width="40" height="26" rx="3"/><text class="ts c" x="130" y="133">write</text>
      <rect class="blue" x="270" y="116" width="40" height="26" rx="3"/><text class="ts c" x="290" y="133">write</text>
      <rect class="blue" x="430" y="116" width="40" height="26" rx="3"/><text class="ts c" x="450" y="133">write</text>
      <path class="line dash" d="M590 80 V150"/>
      <text class="text-green" x="600" y="122">done</text>
      <text class="tm" x="110" y="164">The save now takes as long as MD5 alone: writing is hidden behind it.</text>
    </svg>
    <figcaption>Each big block is hashed and written at the same time.</figcaption>
    </figure>

Seven real files with checksums, saved uncompressed, went from 650 to 542 ms. Each is now as
fast as MD5 itself: the 158 MB MMS FPI burst file saves in 162 ms, which is its size at
970 MB/s.

Reading a file
--------------

:doc:`performance` already explains why reading is fast. Here is how each part works.

Opening reads only the headers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``pycdfpp.load`` maps the file in memory and parses only the records that describe it:
variables, attributes, where the values are. Values are read when you first touch them
(``lazy_load=True``, the default). Opening the 178 MB MMS FPI file takes 0.6 ms.

NASA's library, used by ``spacepy``, hashes the whole file to check its MD5 checksum every
time it opens one: 280 ms for the same file. CDFpp does not check checksums when reading.

Decompressing blocks in parallel
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mission files compress a variable in many independent blocks: an MMS FPI distribution
variable has 640. CDFpp decompresses them on all cores at once, each into its own slice of the
output, with `libdeflate <https://github.com/ebiggers/libdeflate>`_ (1.5 to 1.7 times faster
than zlib on these files).

One trap there: libdeflate may write scratch bytes anywhere in the output space it is given.
So each block gets exactly its own records' space, never "the rest of the buffer". Otherwise
parallel blocks overwrite each other's first values. Synthetic data never showed it. Real FGM
data does, and is a test fixture now.

When saving, compressed variables are cut into blocks of about 256 KB, compressed in parallel.
On CDAWeb data, the compressed size stays within 0.1% of one block per variable.

Huge pages, touched by one thread first
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Linux, big buffers ask for 2 MB huge pages: filling them on one thread is 2 to 3 times
faster than with 4 KB pages, because there are 512 times fewer page faults. But they hide a
trap for parallel code:

1. Each thread decompresses its blocks into its own part of the buffer.
2. Consecutive blocks often fall into the same fresh 2 MB page.
3. Several threads then fault the same page at the same time. The kernel zeroes a 2 MB page
   for each of them, keeps one, and throws the others away.
4. A 100 MB load spent 900 ms of CPU zeroing pages, and took 82 ms instead of 27.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 250" role="img" aria-label="Huge page fault race and the fix">
      <text class="tb" x="20" y="22">One fresh 2 MB huge page, four threads</text>
      <text class="t r" x="110" y="66">before</text>
      <rect class="panel" x="120" y="44" width="400" height="36" rx="4"/>
      <line class="faint" x1="220" y1="44" x2="220" y2="80"/><line class="faint" x1="320" y1="44" x2="320" y2="80"/><line class="faint" x1="420" y1="44" x2="420" y2="80"/>
      <text class="ts c" x="170" y="66">thread 1</text><text class="ts c" x="270" y="66">thread 2</text>
      <text class="ts c" x="370" y="66">thread 3</text><text class="ts c" x="470" y="66">thread 4</text>
      <path class="arrow" d="M530 62 H566"/>
      <rect class="red" x="570" y="36" width="60" height="16" rx="2"/>
      <rect class="red" x="576" y="48" width="60" height="16" rx="2"/>
      <rect class="red" x="582" y="60" width="60" height="16" rx="2"/>
      <rect class="green" x="588" y="72" width="60" height="16" rx="2"/>
      <text class="ts" x="656" y="56">4 pages zeroed,</text>
      <text class="ts" x="656" y="70">1 kept</text>
      <text class="t r" x="110" y="146">after</text>
      <rect class="green" x="120" y="110" width="400" height="20" rx="4"/>
      <text class="ts c" x="320" y="124">one thread writes 1 byte per 4 KB: the page is zeroed once</text>
      <rect class="panel" x="120" y="136" width="400" height="36" rx="4"/>
      <line class="faint" x1="220" y1="136" x2="220" y2="172"/><line class="faint" x1="320" y1="136" x2="320" y2="172"/><line class="faint" x1="420" y1="136" x2="420" y2="172"/>
      <text class="ts c" x="170" y="158">thread 1</text><text class="ts c" x="270" y="158">thread 2</text>
      <text class="ts c" x="370" y="158">thread 3</text><text class="ts c" x="470" y="158">thread 4</text>
      <path class="arrow" d="M530 154 H566"/>
      <rect class="green" x="570" y="146" width="60" height="16" rx="2"/>
      <text class="ts" x="640" y="158">1 page, no fault</text>
      <text class="tm" x="20" y="210">Touching every page from one thread first costs about 10 ms per 100 MB,</text>
      <text class="tm" x="20" y="230">and keeps huge pages' speed for the threads that follow.</text>
    </svg>
    <figcaption>Prefaulting before going parallel: 100 MB load from 81 to 28 ms.</figcaption>
    </figure>

The kernel's own counters (``thp_fault_alloc``) don't show the wasted pages: they count only
the page that was kept. ``perf`` showed it: kernel time zeroing huge pages, from several
threads at once.

Converting time
---------------

CDF has three time types. CDFpp converts all of them to nanoseconds since 1970, which is what
numpy's ``datetime64[ns]`` stores. The conversion has to be exact, to the nanosecond, and
fast: a time axis can hold hundreds of millions of values.

.. list-table::
   :header-rows: 1
   :widths: 18 30 52

   * - Type
     - Stored as
     - To nanoseconds since 1970
   * - CDF_EPOCH
     - one double: milliseconds since year 0, no leap seconds
     - ``floor((ms - 62 167 219 200 000) × 10⁶)``
   * - CDF_EPOCH16
     - two doubles: seconds since year 0, and picoseconds
     - ``(s - 62 167 219 200) × 10⁹ + ps / 1000``
   * - TT2000
     - one int64: nanoseconds since J2000, in TT, leap seconds included
     - ``tt2000 + C - (TAI-UTC)(t)``, with ``C`` a constant and TAI-UTC from the leap
       second table

All three run on SIMD registers: 2 values at a time with SSE2, 4 with AVX2, 8 with AVX-512.
Each type has its own difficulty. ARM uses its own NEON code: see `On Apple Silicon`_.
WebAssembly uses these kernels with two lanes: see `In the browser`_.

Choosing the instruction set at run time
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A wheel must run on any x86-64 CPU, but should use AVX-512 where there is one. So the
conversions are compiled three times, each in its own object with its own compiler flags. At
the first call, `xsimd <https://github.com/xtensor-stack/xsimd>`_ asks the CPU what it
supports, and calls the best version.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 262" role="img" aria-label="Run-time dispatch of the time conversions">
      <text class="tb" x="20" y="22">Built once per instruction set, chosen at run time</text>
      <rect class="panel" x="20" y="40" width="200" height="150" rx="8"/>
      <text class="tb c" x="120" y="64">chrono_arch.cpp</text>
      <rect class="violet" x="36" y="78" width="168" height="28" rx="4"/>
      <text class="tmono c" x="120" y="97">-mavx512bw … → AVX-512</text>
      <rect class="blue" x="36" y="112" width="168" height="28" rx="4"/>
      <text class="tmono c" x="120" y="131">-mavx2 → AVX2</text>
      <rect class="green" x="36" y="146" width="168" height="28" rx="4"/>
      <text class="tmono c" x="120" y="165">-msse2 → SSE2</text>
      <path class="arrow" d="M220 115 H276"/>
      <rect class="orange" x="280" y="80" width="170" height="70" rx="8" stroke-width="1.5"/>
      <text class="text-orange c" x="365" y="106">xsimd::dispatch</text>
      <text class="t c" x="365" y="126">asks the CPU (CPUID),</text>
      <text class="t c" x="365" y="142">calls the best version</text>
      <path class="arrow" d="M450 115 H506"/>
      <rect class="panel" x="510" y="40" width="230" height="150" rx="8"/>
      <text class="tb" x="526" y="64">at each call</text>
      <text class="t" x="526" y="88">fewer than 8 values → scalar</text>
      <text class="t" x="526" y="112">1 M values per thread or more →</text>
      <text class="t" x="526" y="130">  split over threads</text>
      <text class="t" x="526" y="154">SSE2 only → scalar (2 lanes</text>
      <text class="t" x="526" y="172">  don't pay for the extra work)</text>
      <rect class="red" x="20" y="204" width="720" height="50" rx="6"/>
      <text class="t" x="34" y="224">The scalar fallback is compiled separately, without SIMD flags. An inline copy in the AVX-512 object</text>
      <text class="t" x="34" y="242">could be the one the linker keeps: an illegal instruction on every CPU without AVX-512.</text>
    </svg>
    <figcaption>ARM has NEON code of its own (see On Apple Silicon). WebAssembly builds these kernels for its SIMD (see In the browser).</figcaption>
    </figure>

There is one trap in this scheme, and a test guards it (``tests/simd_isolation``):

1. The SIMD code falls back to scalar code for the last few values.
2. If that scalar code is an inline function, every per-instruction-set object emits its own
   copy, compiled with that object's flags.
3. The linker keeps one copy for the whole program, any of them. If it keeps the AVX-512
   one, every CPU without AVX-512 crashes with an illegal instruction.

So the scalar fallback is compiled in a separate file, without SIMD flags, and the SIMD objects
only call it.

TT2000: leap seconds
~~~~~~~~~~~~~~~~~~~~

TT2000 counts real seconds, leap seconds included. UTC, and ``datetime64``, do not. So the
conversion subtracts TAI-UTC, the number of leap seconds so far: 10 s in 1972, then one more
at each leap second, up to 37 s since 2017. Before 1972, TAI-UTC drifted by fractions of a
second, with a formula of its own.

**Most data is recent and sorted.** If the first value is after the last leap second
(2017-01-01), CDFpp bets that all values are, and converts each with a single addition:
``ns = tt2000 + (C - 37 s)``. One comparison per value checks the bet, folded into a single
"all lanes passed" mask. If the bet loses, the general path redoes the array. The same
comparison also catches dates after 2262, whose nanoseconds since 1970 overflow int64: their
sum wraps around to a value below 2017, so they fail the bet and become NaT.

**The general path** handles any order. It cannot look up the leap second table per lane, so
it walks the table instead, for all lanes at once:

1. Start every lane at TAI-UTC = 37 s, the newest value.
2. Lanes earlier than the current leap second get one leap second less. Step to the previous
   leap second.
3. Stop as soon as no lane is earlier than the current leap second.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 270" role="img" aria-label="Walking the leap second table for four lanes">
      <text class="tb" x="20" y="22">Four values in one AVX2 register, walking the leap seconds backwards</text>
      <text class="ts c" x="200" y="46">lane 0</text><text class="ts c" x="320" y="46">lane 1</text>
      <text class="ts c" x="440" y="46">lane 2</text><text class="ts c" x="560" y="46">lane 3</text>
      <text class="t r" x="130" y="72">values</text>
      <rect class="blue" x="150" y="56" width="100" height="24" rx="3"/><text class="t c" x="200" y="73">2019</text>
      <rect class="blue" x="270" y="56" width="100" height="24" rx="3"/><text class="t c" x="320" y="73">2016-10</text>
      <rect class="blue" x="390" y="56" width="100" height="24" rx="3"/><text class="t c" x="440" y="73">2015-03</text>
      <rect class="blue" x="510" y="56" width="100" height="24" rx="3"/><text class="t c" x="560" y="73">2021</text>
      <text class="t r" x="130" y="110">start</text>
      <rect class="green" x="150" y="94" width="100" height="24" rx="3"/><text class="t c" x="200" y="111">37 s</text>
      <rect class="orange" x="270" y="94" width="100" height="24" rx="3"/><text class="t c" x="320" y="111">37 s</text>
      <rect class="orange" x="390" y="94" width="100" height="24" rx="3"/><text class="t c" x="440" y="111">37 s</text>
      <rect class="green" x="510" y="94" width="100" height="24" rx="3"/><text class="t c" x="560" y="111">37 s</text>
      <text class="ts" x="622" y="110">&lt; 2017-01? −1 s</text>
      <text class="t r" x="130" y="148">step 1</text>
      <rect class="green" x="150" y="132" width="100" height="24" rx="3"/><text class="t c" x="200" y="149">37 s</text>
      <rect class="green" x="270" y="132" width="100" height="24" rx="3"/><text class="t c" x="320" y="149">36 s</text>
      <rect class="orange" x="390" y="132" width="100" height="24" rx="3"/><text class="t c" x="440" y="149">36 s</text>
      <rect class="green" x="510" y="132" width="100" height="24" rx="3"/><text class="t c" x="560" y="149">37 s</text>
      <text class="ts" x="622" y="148">&lt; 2015-07? −1 s</text>
      <text class="t r" x="130" y="186">step 2</text>
      <rect class="green" x="150" y="170" width="100" height="24" rx="3"/><text class="t c" x="200" y="187">37 s</text>
      <rect class="green" x="270" y="170" width="100" height="24" rx="3"/><text class="t c" x="320" y="187">36 s</text>
      <rect class="green" x="390" y="170" width="100" height="24" rx="3"/><text class="t c" x="440" y="187">35 s</text>
      <rect class="green" x="510" y="170" width="100" height="24" rx="3"/><text class="t c" x="560" y="187">37 s</text>
      <text class="ts" x="622" y="186">&lt; 2012-07? none: stop</text>
      <rect class="orange" x="150" y="214" width="18" height="14" rx="2"/>
      <text class="t" x="176" y="226">still before the current leap second</text>
      <rect class="green" x="440" y="214" width="18" height="14" rx="2"/>
      <text class="t" x="466" y="226">settled</text>
      <text class="tm" x="20" y="256">Each step is one comparison and one masked addition for all lanes: no table lookup per lane, no branch per value.</text>
    </svg>
    <figcaption>Recent data stops after a few steps. 1972 data walks all 27: the price of having no branches.</figcaption>
    </figure>

Two kinds of values go to the scalar code, a register at a time:

* Values before 1972, where TAI-UTC is a drifting fraction of a second.
* The special values: fill (``INT64_MIN``), pad (``INT64_MIN + 1``) and illegal
  (``INT64_MIN + 3``), which become NaT. They are all far before 1972, so the same test catches
  them.

The scalar code has its own tricks. It remembers where it was in the leap second table, since
sorted data rarely moves. And it tests "usual value" with one unsigned comparison:
``uint64(t - first_usual) <= usual_span`` is true only between the special values and the
last representable date. A second comparison, for the one real date hidden among the special
values (``INT64_MIN + 2``, in 1707), cost 8% in that loop. So that case moved to a function
called only for unusual values.

CDF_EPOCH: exact, without 64-bit multiplies
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

CDF_EPOCH looks easy: subtract an offset, multiply by a million. Two things make it hard.

1. **A double product is not exact.** Milliseconds since 1970 are about 1.6 × 10¹² today.
   Times 10⁶, that is 1.6 × 10¹⁸ nanoseconds, above 2⁶⁰. A double has 53 bits of mantissa,
   so at that size it can only hold multiples of 256 ns. The product rounds, and times move by
   up to 128 ns.
2. **AVX2 has no instruction for the exact way.** Converting a double to int64, and
   multiplying two int64, both arrived with AVX-512DQ. On AVX2, each would cost a sequence of
   instructions per lane.

The solution is to split the number so that every product stays small enough to be exact.

1. ``x = ms - offset`` is exact. Both numbers are within a factor 2 of each other, so the
   subtraction loses nothing.
2. ``high = floor(x / 8192)`` and ``low = x - high × 8192``. Dividing by a power of two is
   exact, so ``low`` is exact too, between 0 and 8192.
3. ``high × 10⁶`` is an integer below 2⁵¹: exact.
4. ``ms`` itself, counted from year 0, is above 2⁴⁵. So its fraction, and ``x``'s, is a
   multiple of 2⁻⁷. Then ``low`` is a whole number of 2⁻⁷ below 2²⁰ of them, and ``low × 10⁶``
   needs at most 40 significant bits: exact too. Its floor is below 2³³.
5. The result is ``(high × 10⁶) << 13`` plus ``floor(low × 10⁶)``: exactly
   ``floor(x × 10⁶)``.

The last piece converts those integer-valued doubles to int64 without a conversion
instruction: the **magic number** trick.

.. raw:: html

    <figure class="cdfpp-diagram">
    <svg viewBox="0 0 760 260" role="img" aria-label="The magic number conversion from double to int64">
      <text class="tb" x="20" y="22">Adding 1.5 × 2⁵² puts an integer straight into the mantissa bits</text>
      <text class="ts c" x="137" y="46">sign</text>
      <text class="ts c" x="216" y="46">exponent (11 bits)</text>
      <text class="ts c" x="505" y="46">mantissa (52 bits)</text>
      <text class="t r" x="120" y="74">1.5 × 2⁵²</text>
      <rect class="panel" x="128" y="58" width="18" height="26"/><text class="tmono c" x="137" y="76">0</text>
      <rect class="violet" x="146" y="58" width="140" height="26"/><text class="tmono c" x="216" y="76">2⁵² (fixed)</text>
      <rect class="orange" x="286" y="58" width="22" height="26"/><text class="tmono c" x="297" y="76">1</text>
      <rect class="panel" x="308" y="58" width="422" height="26"/><text class="tmono c" x="519" y="76">000 … 000</text>
      <text class="t r" x="120" y="124">v + 1.5 × 2⁵²</text>
      <rect class="panel" x="128" y="108" width="18" height="26"/><text class="tmono c" x="137" y="126">0</text>
      <rect class="violet" x="146" y="108" width="140" height="26"/><text class="tmono c" x="216" y="126">2⁵² (fixed)</text>
      <rect class="orange" x="286" y="108" width="22" height="26"/><text class="tmono c" x="297" y="126">1</text>
      <rect class="green" x="308" y="108" width="422" height="26"/><text class="tmono c" x="519" y="126">v, as a plain integer (|v| &lt; 2⁵¹)</text>
      <text class="tm" x="128" y="160">With the exponent fixed at 2⁵², one unit of the mantissa is exactly 1: the addition writes v</text>
      <text class="tm" x="128" y="178">into the low bits, rounding nothing since v is already an integer.</text>
      <rect class="blue" x="128" y="196" width="602" height="40" rx="6"/>
      <text class="tmono c" x="429" y="221">int64 v = bits(v + magic) − bits(magic)        ← one add, one integer subtract</text>
    </svg>
    <figcaption>Negative values work too: they borrow from the 1 bit just below the exponent.</figcaption>
    </figure>

Out-of-range values (fill ``-1e31``, pad ``0.0``, NaN, dates outside 1677–2262) give garbage
in their lanes. A comparison mask, computed alongside, replaces them with NaT at the end. No
branch, and no undefined behaviour: the scalar version swaps them for a harmless value before
converting, because converting NaN to an integer is undefined in C++.

CDF_EPOCH conversions became exact in CDFpp 0.14. The scalar version got 2 times slower
then. The SIMD version, rebuilt this way, is faster than the old, inexact scalar one.

CDF_EPOCH16
~~~~~~~~~~~

An EPOCH16 value is two doubles side by side: seconds, then picoseconds. A *gather* load reads
the even doubles into one register and the odd ones into another. Whole seconds are below 2³⁴
and nanoseconds below 10⁹, so both convert with the magic number. A mask turns invalid values
into NaT, as for CDF_EPOCH.

Alignment
~~~~~~~~~

Aligned loads and stores are a little faster on some CPUs. Each conversion checks the input
and output addresses and picks aligned or unaligned instructions for each. When both are
misaligned by the same amount, TT2000 converts a few values with the scalar code first, so the
rest of the array is aligned.

How fast
~~~~~~~~

Values per second, one thread, data in cache (1 000 values) and not (64 M values, 1 GB):

.. list-table::
   :header-rows: 1

   * - Ryzen 7 5800X, AVX2
     - 1 K values
     - 64 M values
   * - TT2000, scalar
     - 0.97 × 10⁹
     - 0.92 × 10⁹
   * - TT2000, SIMD
     - **2.9 × 10⁹**
     - 1.3 × 10⁹
   * - CDF_EPOCH, scalar (exact)
     - 1.1 × 10⁹
     - 0.95 × 10⁹
   * - CDF_EPOCH, SIMD (exact)
     - **2.6 × 10⁹**
     - 1.4 × 10⁹

With AVX-512 (Ryzen 7 7840U), TT2000 reaches about 8 × 10⁹ values per second in cache. At
64 M values, every SIMD version drops to the speed of memory: 8 bytes in, 8 bytes out per
value. Big arrays are split over threads for that reason. The README has the full tables.

From datetime64 back to CDF time
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Writing a ``datetime64`` time axis converts the other way. TT2000 uses the same leap second
walk, from the UTC side of the table: a one-addition path for dates after 2017, and the
per-lane walk otherwise, with AVX2 or AVX-512. CDF_EPOCH and EPOCH16 need an exact 64-bit
division by 10⁶ or 10⁹, which AVX2 can't do in a register: they stay scalar. All three run
on threads for big arrays.

That rewrite also fixed four wrong results, all of them now tested:

1. NaT became a real date: 2262 as TT2000, 1677 as CDF_EPOCH. It now becomes each type's
   fill value, which reads back as NaT.
2. Dates before 1970 lost their EPOCH16 values: picoseconds came out negative, which reads
   back as NaT. They now round down to the second, with picoseconds between 0 and 10¹².
3. Dates before 1970 came out one millisecond late as CDF_EPOCH: rounded toward zero instead
   of down.
4. Dates before 1707, which TT2000 can't hold, overflowed. They now become TT2000's illegal
   value, as with NASA's library.

Converting 1.2 M values from Python (``pycdfpp.to_tt2000``), same machine:

.. list-table::
   :header-rows: 1

   * - Time axis
     - Before
     - After
   * - Sorted, 2019 (after the last leap second)
     - 1.36 ms
     - 1.18 ms
   * - Sorted, 1995
     - 6.86 ms
     - 2.12 ms
   * - Sorted, 2008 to 2017 (leap seconds inside)
     - 8.27 ms
     - 1.39 ms
   * - Shuffled, 2008 to 2017
     - 12.19 ms
     - 2.80 ms
   * - Shuffled, 1972 to 2262
     - 3.77 ms
     - 4.90 ms

Most of the 2019 time is not the conversion: the kernel zeroes the fresh output array.
Shuffling dates across three centuries is the walk's worst case: most lanes of a register
wait for the oldest one, while the old scalar code handled each date after 2017 at once. Real
time axes are sorted, or at least from one era.

Fixing the rounding costs a little for CDF_EPOCH and EPOCH16: rounding down needs the sign of
the remainder, one more multiplication per value. 1.2 M values take 2.0 ms instead of 1.5 ms
as CDF_EPOCH, 2.6 ms instead of 2.1 ms as EPOCH16.

On Apple Silicon
----------------

Measured on an Apple M2 (4 performance and 4 efficiency cores, 16 KB pages), macOS 26,
Apple clang 21, APFS on the internal SSD. Before this work, pycdfpp lost one task of the
comparison to spacepy and cdflib on this machine: writing an uncompressed file, 31.5 ms
against 12.7 ms. None of the causes showed on Linux.

Build like the wheels
~~~~~~~~~~~~~~~~~~~~~

Meson turns on libc++'s hardening (``_LIBCPP_HARDENING_MODE_FAST``) when ``NDEBUG`` isn't
defined, which is the default of ``meson setup --buildtype=release``. Every ``std::span``
access is then bounds checked, and xsimd's alignment ``assert`` runs on every load. Wheels
are built by meson-python with ``-Db_ndebug=if-release``, without either. Benchmark with
``-Db_ndebug=if-release`` too, or the numbers are not the ones users get. libstdc++ only
checks in debug builds, so Linux builds don't show the difference.

Writing a file without std::fstream
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

libc++'s ``std::fstream`` sends big writes through its small stdio buffer. libstdc++ hands
them to the kernel at once. Writing 27 MB:

.. list-table::
   :header-rows: 1

   * - Chunk
     - 64 KB
     - 1 MB
     - 27 MB
   * - ``std::fstream``, libc++
     - 27.1 ms
     - 27.5 ms
     - 28.7 ms
   * - ``write()``
     - 8.6 ms
     - 8.4 ms
     - 8.1 ms

On macOS, CDFpp writes files with ``open``, ``write`` and ``ftruncate``. The rest is unchanged:
an existing file is written over in place, then cut to size. Linux and Windows keep
``std::fstream``. Saving the uncompressed FGM file: 29.1 → 10.1 ms, the time of a Python
``f.write()`` of the same bytes.

No C++ exception to end a loop
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Listing the attributes of the MMS FPI file took 3.4 ms here, against 0.6 ms on the Ryzen.
``sample`` showed most of it in ``libunwind`` and ``dyld``: C++ exceptions. Throwing one on
macOS looks up the unwind tables of each frame in the loaded images, tens of microseconds.

* ``list(attribute)`` used ``__getitem__`` until it raised ``IndexError``: one exception per
  attribute.
* pybind11's ``make_iterator``, used by ``for name in cdf`` and ``.items()``, ends every loop
  by throwing ``StopIteration`` from C++.

Attributes now have an ``__iter__``, and collections iterate over a Python list of their
names, or of (name, value) pairs. Python ends those loops without any exception. Values are
still bound to their CDF, as before, and a loop no longer sees variables added while it runs.
Opening the file and reading every attribute: 3.4 → 0.5 ms.

Time conversions with NEON
~~~~~~~~~~~~~~~~~~~~~~~~~~

Every aarch64 CPU has NEON, so ``src/arch/arm/chrono.cpp`` is built without run-time
dispatch. It doesn't use xsimd. NEON registers hold only two 64-bit lanes, and its generic
code missed what AArch64 offers and x86 lacks:

* ``fcvtms``/``fcvtps``: double to int64, rounding down or up, in one instruction.
* ``frintz`` and a fused multiply-add on every core.
* ``ld2``: loads pairs of doubles split into two registers, the EPOCH16 layout.

So each type has its own kernel:

* **TT2000, both ways.** Between two leap seconds, a conversion is one addition. Values are
  checked 32 at a time to be in the interval of the first one. That is one unsigned comparison
  per value, ``uint64(v - first) <= last - first``, and one reduction per block. A block that
  crosses a leap second, or holds fill values, goes to the scalar code. The next block takes
  the interval of its own first value. Sorted data converts with one addition per value,
  whatever its year, without walking the table per lane as on x86.
* **CDF_EPOCH.** ``p = x × 10⁶`` rounds, but ``e = p - x × 10⁶``, computed with one fused
  multiply-add, is exact. Below 2⁵², ``p`` is exact and ``e = 0``. Above, ``p`` is whole.
  ``floor(x × 10⁶) = floor(p) - ceil(e)`` in both cases: two conversions, no 2¹³ split.
* **EPOCH16.** ``ld2`` splits seconds and picoseconds. The same product trick handles whole
  seconds × 10⁹.

Apple's cores need two more things:

1. NEON operations take 2 cycles or more. With 2 lanes, a loop that ANDs each result into one
   accumulator waits on it: 1 value per cycle at most. Each step works on 4 independent
   registers, and the "all in the interval" mask is reduced once per 32 values. Moving a
   vector to a general register (``uminv``, ``fmov``) is slow too.
2. Big arrays convert 1.5 times faster when each store writes a whole 64-byte cache line:
   one 4-register ``st1`` rather than two ``stp``. Our reading is that the core then doesn't
   read the line before writing it. Loads are the other way round: pairs (``ldp``) beat the
   4-register ``ld1``.

Values per second, one thread, data in the L2 cache (1 M values) and not (64 M):

.. list-table::
   :header-rows: 1

   * - Apple M2
     - scalar, 1 M
     - NEON, 1 M
     - scalar, 64 M
     - NEON, 64 M
   * - TT2000, 2019 (after the last leap second)
     - 5.2 × 10⁹
     - **6.1 × 10⁹**
     - 3.0 × 10⁹
     - **4.3 × 10⁹**
   * - TT2000, 1972 to 2036
     - 0.67 × 10⁹
     - **6.1 × 10⁹**
     - 0.68 × 10⁹
     - **4.2 × 10⁹**
   * - CDF_EPOCH
     - 1.7 × 10⁹
     - **2.4 × 10⁹**
     - 1.7 × 10⁹
     - **2.3 × 10⁹**
   * - EPOCH16
     - 0.67 × 10⁹
     - **1.2 × 10⁹**
     - 0.67 × 10⁹
     - **1.2 × 10⁹**
   * - datetime64 to TT2000, 2019
     - 1.1 × 10⁹
     - **4.4 × 10⁹**
     - 1.1 × 10⁹
     - **3.8 × 10⁹**
   * - datetime64 to TT2000, 1972 to 2036
     - 0.22 × 10⁹
     - **5.2 × 10⁹**
     - 0.22 × 10⁹
     - **4.4 × 10⁹**

Two cases favour the scalar code. Clang vectorizes the scalar loop for recent TT2000 well:
it converts 1 000 values, in the L1 cache, at 8.6 × 10⁹ against 7.0 × 10⁹. A time axis
shuffled across leap seconds goes to the scalar code block after block. Both take
microseconds, and real time axes are sorted. ``pycdfpp.to_tt2000`` on the 1.2 M FGM times:
1.13 → 0.23 ms.

Every result is the scalar code's, bit for bit. ``tests/chrono`` checks it around every leap
second, at every position in a block, and with special values among recent ones. Changing
any interval bound, or the one-sided test of the last interval, fails it.

Where the rest goes
~~~~~~~~~~~~~~~~~~~

* Reading is 85% libdeflate decompression, on every core.
* Writing gzip is libdeflate compression, about 7 of the 8 cores busy. The efficiency cores
  are slower, so the M2 writes the FPI file in 400 ms, against 275 ms on the 16 threads of the
  Ryzen. That is still 11 times faster than spacepy and cdflib here.

Profile on macOS with ``sample <pid> 5 -file out.txt`` while a script loops. Its "Sort by top
of stack" summary is the equivalent of ``perf report --no-children``.


In the browser
--------------

CDFpp runs in the browser twice: the `CDFpp Explorer <https://sciqlop.github.io/CDFpp/>`_,
built with Emscripten, and the Pyodide wheels. Neither can start threads, so everything above
that runs on several cores runs on one there.

The measurements below come from Node 22 running the Explorer's module on the same five
CDAWeb files as before. V8 profiles WebAssembly too: link with ``--profiling-funcs`` to keep
function names, run ``node --cpu-prof``, and sum the self time per function of the
``.cpuprofile`` it writes.

Where the time goes
~~~~~~~~~~~~~~~~~~~

* **Loading** is libdeflate decompression (55%) and its CRC-32 check (15%). WebAssembly has no
  carry-less multiplication, so CRC-32 uses tables.
* **Saving a file with a checksum** is MD5 (33 to 44%), now with no second thread to hide
  behind.
* **Returning a saved file to JavaScript** copies it: 29% of a raw save. The Explorer
  transfers each saved file from its worker to the page, and only a JavaScript
  ``ArrayBuffer`` can be transferred.
* **Gzip saves** are compression, on one core: 1.3 s for the 78 MB MMS FGM file, 3.4 s for
  the 186 MB FPI file.

The profile also showed a bug that wasn't specific to WebAssembly at all.

Variables were copied when a CDF grew
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

11% of a load went to a function named after ``std::variant``'s copy constructor. Its
callers led to ``nomap::operator[]``:

1. A CDF keeps its variables in a ``std::vector``.
2. When a vector grows, it moves its elements only if their move can't throw. Otherwise it
   copies them, to keep its strong exception guarantee.
3. ``Variable`` holds a ``lazy_load_guard``, with a mutex and only a copy constructor. So
   moving a ``Variable`` could throw.
4. Every time the vector grew, it copied every variable already in it, values included.

``lazy_load_guard`` now has a ``noexcept`` move, and a test checks that moving a ``Variable``
can't throw. Native builds gain as much:

.. list-table::
   :header-rows: 1

   * - Ryzen 7 5800X
     - Before
     - After
   * - Eager load, MMS FGM
     - 57 ms
     - 25 ms
   * - Eager load, MMS FPI
     - 108 ms
     - 65 ms
   * - Eager load, Wind MFI
     - 29 ms
     - 17 ms
   * - Adding 300 variables of 800 KB from Python
     - 193 ms
     - 102 ms

One file got slower: THEMIS ESA, 383 small variables, 40.6 to 43.2 ms. Its copies are gone,
but more time goes to the kernel zeroing fresh pages.

WebAssembly SIMD
~~~~~~~~~~~~~~~~

The Explorer's module was built for baseline WebAssembly, without SIMD. It is now built with
``-msimd128``. Every browser has WebAssembly SIMD since 2023 (Safari 16.4). The compiler then
vectorizes byte swaps, deflate and other loops on its own:

.. list-table::
   :header-rows: 1

   * - Task (Node 22, Ryzen 7 5800X)
     - Without SIMD
     - With SIMD
   * - Gzip save, MMS FPI
     - 3.77 s
     - 3.36 s
   * - Gzip save, THEMIS ESA
     - 695 ms
     - 579 ms
   * - Load, Wind MFI (big-endian)
     - 25.3 ms
     - 16.6 ms
   * - Raw save, THEMIS ESA (big-endian)
     - 58 ms
     - 51 ms
   * - Load, MMS FPI (decompression only)
     - 564 ms
     - 560 ms

The time conversions build the x86 kernels for xsimd's WebAssembly target. WebAssembly SIMD
has ``floor``, ``trunc``, 64-bit compares and selects, so they map one to one, unlike on NEON.
They matter for one reason. Left to itself, the compiler vectorized the CDF_EPOCH loop badly:
WebAssembly SIMD has no double to int64 conversion, so it converted lane by lane inside vector
code, and the Wind MFI time axis took 3.8 ms instead of 2.8. The kernel, with the magic number
conversion, takes 2.5 ms. TT2000 doesn't change: the compiler already vectorized its fast path.
``wasm_chrono_simd`` checks all four conversions against the scalar code, bit for bit.

The Pyodide wheels are built by Pyodide's tools, without ``-msimd128``: they keep the scalar
code.

Measuring it yourself
---------------------

The tools behind every number of this page, on Linux.

**Where the time goes.** Sample the call stacks, then look at the hottest instructions:

.. code-block:: bash

    perf record -g -- python my_script.py
    perf report --no-children --percent-limit 1
    perf annotate --stdio <symbol>

**Why it is slow.** The CPU counts its own events. On AMD Zen 3 (Intel has equivalents,
``perf list`` shows them):

.. code-block:: bash

    perf stat -e cycles,instructions,ex_ret_brn_misp \
              -e l1_data_cache_fills_all,ls_l1_d_tlb_miss.all \
              -e de_dis_dispatch_token_stalls1.store_queue_rsrc_stall \
              -e de_dis_dispatch_token_stalls1.load_queue_rsrc_stall -- ./bench

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Counter
     - What it told us here
   * - instructions / cycles
     - Below 2: something waits (MD5's chain). Near 4: the code is instruction-bound
       (the transpose, before SIMD blocks).
   * - store queue stalls
     - The old transpose: stores to scattered lines, 48% of cycles blocked.
   * - L1 data cache fills
     - Why 32-byte tiles beat 128-byte ones.
   * - branch mispredictions
     - Negligible everywhere on this page: not worth branch-free tricks beyond SIMD.
   * - instructions, alone
     - Doesn't depend on machine load: two builds that run the same code count the same.
       That settled a 10% "regression" that was only noise.

**Avoid lying benchmarks.**

1. Repeat, keep the best or the median, and interleave the old and new versions: other
   programs change the machine's speed over minutes.
2. Between two writes of a file, ``sync`` and delete it, outside the timed part. Otherwise
   the disk is still writing the previous run, and times swing from 17 to 48 ms.
3. Benchmark real files. ``benchmarks/python_libs/compare.py`` compares pycdfpp, spacepy and
   cdflib on real CDAWeb files.
4. Compare builds made the same way. A PyPI wheel and a local build use different compilers.

Ideas that were not kept
------------------------

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Idea
     - Why not
   * - Keep freed big buffers, to skip the kernel zeroing new pages
     - 28 → 22 ms per 100 MB read. But memory held by a library is never returned, and users
       see it as a leak.
   * - Hand-written AVX2 / AVX-512 transpose
     - About 8% end to end, for some files, for a run-time dispatch of its own. See
       `Why not AVX2 or AVX-512?`_
   * - ``fallocate``, ``mmap``, ``O_DIRECT``, parallel ``pwrite``
     - No faster than one ``write()``. See `File system tricks that did not help`_.
   * - Forcing the file to disk, overlapping the disk with the CPU
     - Only useful when the caller waits for the disk. Planned as an option:
       `issue #127 <https://github.com/SciQLop/CDFpp/issues/127>`_.
   * - Turning assertions off in the WebAssembly build
     - No measurable difference on any file or task.
   * - Threads in the browser
     - They need ``SharedArrayBuffer``, which needs headers GitHub Pages can't send. Gzip saves
       stay on one core there. See `In the browser`_.
   * - xsimd's generic code on NEON
     - Its ``floor`` and ``trunc`` are emulated there: CDF_EPOCH ran 2.3 times slower than
       scalar. See `On Apple Silicon`_.
   * - Parallel ``pwrite``, ``F_NOCACHE``, ``F_PREALLOCATE``, ``mmap`` on APFS
     - 27 MB in 8.2 to 9.7 ms, against 8.6 ms for one ``write()``: noise.
   * - More threads for time conversions on an M2
     - One core already streams about 70 GB/s; two threads gain nothing.
