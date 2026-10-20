# Shared cluster TPTP example

Runs E on two problems stored entirely on the cluster under
`/share/TPTP/TPTP-v9.2.1/Problems/`. No local problem files are required or uploaded.
`problem-globs.txt` contains versioned `tptp://` paths; change these to select
other problems or use globs. Each problem uses the selected TPTP version's axioms.

Open this example in the app or run `make prepare-submit`. The solver and
runsolver are built on the cluster using the existing E build recipe. To edit
in the app, duplicate this workflow first. The reservation field beside the
partition applies to both builds and solver tasks. For CLI use, set
`SLURMY_RESERVATION=E192-04` only when you have a valid reservation.
