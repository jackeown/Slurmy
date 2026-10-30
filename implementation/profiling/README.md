# Job Page Load Profile — 31 October 2026

Measured a running job with 78,792 solver-problem pairs and 66 batches. These are
individual samples on the current cluster, not promises about typical performance.
The three live probes were read-only; further CPU/view profiling reused the
captured payload locally, without more cluster reads.

| Stage | First cold sample | Slow cold sample |
|---|---:|---:|
| Reading progress files | 4.025 s | 2.060 s |
| Reading/streaming result indexes | 0.892 s | **17.031 s** |
| Reading/streaming the manifest | 0.435 s | 0.424 s |
| SLURM queue query | 0.018 s | 0.018 s |
| SLURM accounting query | 0.014 s | 0.016 s |
| Total SSH collection | **5.995 s** | **20.353 s** |
| Local decompression | 0.121 s | 0.122 s |
| Local parsing | 0.814 s | 0.852 s |

The payload was 76.9–77.7 MB uncompressed and 3.75–3.96 MB compressed. Most
uncompressed bytes were the manifest (72.2 MB), which is omitted on subsequent
incremental refreshes. A separate local-only API replay measured 0.897 s for the
first parse/page build and 0.013 s with the local snapshot cached. Browser
measurements found navigation about 0.33–0.41 s and a cached data request about
0.024 s. These warm measurements do not include waiting for a fresh snapshot.

## Isolating Storage From the VPN

A third probe read result indexes on the head node and discarded their contents
there, returning only timing statistics. Reading 35 files totaling 4,793,364 bytes
consumed **39.279 s wall time but only 0.011 s CPU time**. File listing took 0.010 s.

| File size | Open time | Read time |
|---|---:|---:|
| 167,508 bytes | 0.002 s | **15.847 s** |
| 142,187 bytes | 0.003 s | **9.708 s** |
| 127,802 bytes | 0.006 s | **6.608 s** |
| 123,165 bytes | 0.002 s | **3.756 s** |

The dominant observed delay is in the cluster filesystem read path. The user's
VPN cannot explain this third probe's read times, because those file contents
were not transferred to the user's computer. This establishes slow remote reads;
it does not establish whether the underlying cause is storage contention,
filesystem/cache-coherency behavior, or another cluster-side I/O problem.

The main server's per-host queue wait was not measured in these samples. That
wait can add to a cold page load when another collection is already running.
The earlier browser polling delay was real, but removing it could not eliminate
these independently measured storage stalls.

## Changes and Next Targets

Progress files now use one buffered AWK reader rather than a Bash read loop per
file. The second sample's progress stage was faster, but cluster load and file
contents changed between samples; this is not a controlled 2× speedup claim.

The ETA help button also had a theme specificity bug: the custom theme supplied
dark, semi-transparent accent text on a transparent button over a dark panel.
Its foreground/background now use the theme's ordinary readable surface colors.
The fix was verified in the actual custom theme, including the tooltip.

The next larger performance improvement should separate the initial visible
page/summary from full result-index loading. Reducing repeated reads of mutable
batch indexes and persisting validated static definitions are also candidates.
Simply starting more collectors would increase load without addressing the
observed read stalls. Cached data must remain explicitly labeled with its actual
collection time, as the existing cache implementation does.

## Reproducing a Single Profile

From the repository root:

```sh
python3 implementation/profile_monitor.py --host datalab --job JOB_ID --output /tmp/slurmy-timings.json
```

Use `--fresh-connection` to include a cold SSH handshake. Each invocation performs
one read-only collection with a 45-second remote budget. `--save-payload` and
`--python-profile` are optional local outputs; Python profiling reuses the saved
in-memory data and does not issue another collection. The default JSON report
contains timings and counts, not solver commands or problem contents.
