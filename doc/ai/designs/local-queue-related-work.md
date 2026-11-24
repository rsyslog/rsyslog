<!--
.. meta::
   :description: Prior art, novelty assessment and implementation lessons for rsyslog local queue front ends.
   :keywords: rsyslog, upcoming work, sdq1, SPSC, MPMC, task pools, prior art
-->

# Local queue front ends: related work and synthesis

| Metadata | Value |
|---|---|
| Status | Research supporting upcoming work; no runtime implementation claim |
| Research / review date | 2026-09-14 |
| Design | [Local queue front ends with shared overflow](local-queue-frontends.md) |
| Plan | [Staged implementation plan](local-queue-implementation-plan.md) |
| Method | Research subagent searched primary papers, original implementations, technical descriptions and indexed patents; Astra reviewer independently checked the closest precedent |
| Limit | Bounded architectural prior-art search, not exhaustive novelty proof or patent clearance |

<!-- .. summary-start -->
Private preferred queues, incoming overflow to a central queue, and local-empty
helping have established precedent in combined central/distributed task pools.
The rsyslog proposal specializes those ideas with separate SPSC producer/consumer
pairs, whole-batch routing and existing message-delivery/disk machinery. Prior art
supports plausibility, not correctness, novelty, or an expected speedup percentage.
<!-- .. summary-end -->

## 1. Strongest architectural precedent

Wirz, Süss and Leopold's *A Comparison of Task Pool Variants in OpenMP and a
Proposal for a Solution to the Busy Waiting Problem* describes **sdq1**: bounded
private task queues, incoming tasks sent to a central queue when private space is
exhausted, and central acquisition when the private queue is empty. It uses no
peer-private stealing. The paper's implementations share a pool API and trace sdq1
to Korch and Rauber's earlier implementations. Its private queue belongs to one
thread that both inserts and executes, rather than to separate SPSC producer and
consumer threads. This is direct routing/scheduling precedent, not an exact FE
implementation match. [Original paper, Sections 2.1–2.2](https://www.michaelsuess.net/publications/wirz_suess_leopold_taskpools_06.pdf)

The earlier publication has a 2003 online/2004 journal lineage; use the original
publisher record for provenance, not a claim that the complete rsyslog proposal
was published there. [Korch and Rauber publisher record](https://onlinelibrary.wiley.com/doi/abs/10.1002/cpe.745)

## 2. Complementary precedents

| Source | Established element | Difference from this proposal |
|---|---|---|
| [Affine thread pools, original 2019 implementation](https://www.bartoszsypytkowski.com/thread-safety-with-affine-thread-pools/) | Dedicated workers execute from private MPSC queues and shared MPMC, with no peer stealing | Affinity-based placement rather than overflow; private queue has multiple producers; periodic priority changes |
| [moodycamel original design](https://moodycamel.com/blog/2014/detailed-design-of-a-lock-free-queue) | One queue API implemented with producer-specific subqueues and registration | Consumers search subqueues; no dedicated FE consumer or common overflow tier |
| [Tokio scheduler design](https://tokio.rs/blog/2019-10-scheduler) | Bounded local queues, global work, overflow batching and wake coordination | Executor-owned locals, peer stealing, and exporting existing local work on overflow |
| [Go runtime source](https://go.dev/src/runtime/proc.go) | Local/global scheduling, bounded global acquisition and fairness checks | Processor-owned locals, peer stealing; globally acquired work can populate local run queues |
| [FastFlow implementation](https://github.com/fastflow/fastflow) | SPSC execution channels in streaming computation | Different farm/emitter scheduling; not evidence of the complete FE/BE combination |
| [DPDK ring documentation](https://doc.dpdk.org/guides/prog_guide/ring_lib.html) | Whole-count bulk admission, distinct from partial burst admission | Primitive rather than scheduling/persistence architecture |
| [Rigtorp SPSC source, `try_emplace`](https://github.com/rigtorp/SPSCQueue/blob/master/include/rigtorp/SPSCQueue.h) | Cached consumer index refreshed before declaring full | Single-element primitive; whole-batch publication and shutdown remain our work |

These are references for separate layers: queue primitive, routing policy,
scheduling, and registration. None establishes rsyslog delivery ownership,
transaction handling or persistence correctness. No source's published performance
number is a prediction for this implementation or workload.

## 3. Novelty assessment

The central routing/helping architecture is established. SPSC handoff, dedicated
consumers, whole-count bulk admission and refreshed fullness checks are established
components. The bounded search found no single exact match for the complete
combination of producer-private SPSC execution paths, incoming whole-batch overflow,
shared BE helping, dedicated BE scheduling capacity, and shutdown consolidation
into an existing disk-assisted logical queue. Absence of a match is not evidence
that the combination is novel.

Describe the work as **per-producer SPSC front ends with shared overflow and
idle-consumer helping**, or a specialization of combined central/distributed task
pools for asynchronous message processing. Do not describe it as an established
algorithm called by that descriptive name, or as a newly invented queue algorithm.
The substantive rsyslog engineering is preserving its admission, mutation,
worker-private state, retry and recovery contracts while changing normal routing.

The patent search found a near miss: US20180335957A1 describes source/destination
producer-consumer queues with overflow, but the overflow is source-private and
later pumped back into the original queue, rather than shared MPMC work executed
by helpers. This is a technical distinction only, not a legal claim-scope analysis.
[Published application, paragraphs 0023–0027](https://patents.google.com/patent/US20180335957A1/en)

Search families included `"sdq1" task`, `"private queues" "global queue" overflow`,
`"SPSC" "shared overflow"`, `"SPSC" "fallback" "MPMC"`, `"per-producer" overflow queue`,
`"producer" "dedicated consumer" "shared"`, and patent searches for
`"single producer" "overflow"` and `"private queue" "central queue"`.
Searches first omitted batch/persistence qualifiers to avoid disguising known
architecture with application details. Exhaustive citation-family tracing,
classification-based patent searching and archival code inspection were not done.

## 4. Synthesis for implementation

### 4.1 Separate the layers and their evidence

Keep storage publication, FE/BE routing, consumer scheduling and delivery
completion independently testable. A ring microbenchmark does not show end-to-end
queue progress; scheduling similarity does not establish safe action retries.
Compare MPMC, FE without helping and FE with helping on controlled baselines,
including the common final code with a test-only helping toggle if practical.

### 4.2 Measure stranded private work

The sdq1 paper discusses limits when work remains privately held. Our corresponding
case is a finite burst that fits wholly in one FE, while other consumers are idle
and BE is empty. Helpers cannot reach it. Test completion time and tail latency
against equal-resource MPMC with varied callback cost. This is a motivated
experiment, not a prediction from the old paper's measurements.
[Original paper, Section 2.5](https://www.michaelsuess.net/publications/wirz_suess_leopold_taskpools_06.pdf)

Do not add peer stealing or spill existing FE contents merely because the limitation
exists. Those would change ownership and the accepted policy. Record the tradeoff
and change policy only if workload evidence warrants a separately reviewed revision.

### 4.3 Preserve shared service without overstating progress

Go and Tokio provide examples of periodic shared service. Our initial design uses
a small dedicated BE pool instead. Measure BE oldest age and completion as well as
throughput. Periodic help by locally busy workers remains an optional experiment,
not an accepted requirement; it would not solve work stranded in another FE.
Dedicated scheduling capacity cannot guarantee completion through blocked callbacks.
[Go scheduler](https://go.dev/src/runtime/proc.go),
[Tokio scheduler](https://tokio.rs/blog/2019-10-scheduler)

### 4.4 Batch for the actual target

Shared batch acquisition amortizes coordination, but large helper batches delay
local reconsideration. rsyslog adds a distinct concern: target-preferred batching
may intentionally wait to avoid tiny requests. Measure actual submission, dequeue,
transaction and wire-request sizes separately. Queue batches are not SIMD units:
messages may mutate, branch and submit asynchronously at different stages. No
prior-art scheduler justifies delaying those submissions to manufacture a larger
batch or treating all output transactions as uniform work.

### 4.5 Treat registration and waiting as first-class costs

Producer-specific queues require safe registration retirement/reuse, as moodycamel
illustrates; our dedicated consumers add worker lifetime and resource costs. Test
stable producers and churn separately. Wakeup protocols must cover FE and BE
arrival, producer capacity release, and shutdown. Busy-waiting elimination is also
a central concern of the sdq1 paper. Use independent interleaving tests and measured
notification costs rather than assuming an efficient ring makes scheduling cheap.
[Producer subqueue design](https://moodycamel.com/blog/2014/detailed-design-of-a-lock-free-queue),
[sdq1 paper, Section 2.3](https://www.michaelsuess.net/publications/wirz_suess_leopold_taskpools_06.pdf)

## 5. Reviewer assessment and practical disposition

The informed Astra / medium reviewer independently verified sdq1's central policy
and retained its previous verdict: the staged plan is suitable for gated work,
with no architectural redesign required by this research. It specifically requested
the finite private-burst benchmark, now present in the implementation plan.
Most other lessons were already covered: source-tagged leases, dedicated BE
capacity, producer lifecycle, batching tradeoffs, cached-index refresh and matched
resource comparisons.

Use the research as credibility for the architectural family and as a source of
counterexamples and measurements. The full rsyslog design still requires its own
correctness tests, memory-order reasoning, code reviews and performance evidence.
