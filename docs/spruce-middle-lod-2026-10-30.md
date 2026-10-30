# Spruce middle LOD

- Cause: uniform 10% mesh collapse removed and shrank needle cards. Far billboard and near mesh retained fuller crowns.
- Both imported middle LODs now retain exactly the near mesh's foliage; only wood is reduced further.
- Exporter asserts identical foliage geometry hashes across near/middle LODs.
- Triangles: 1843 near/middle 65,371 / 43,772; 243 near/middle 34,618 / 28,369.
- Offline middle-mesh range capped at 70 m before zoom scaling; billboard takes over beyond that. Near range unchanged. No protocol/collision changes.
- Fixed-distance before/after captures inspected; Release build passed.
- Full old middle range cost 22.00 → 39.11 ms (+17.11 ms, 77.8%) in the medium meadow benchmark; hence the earlier billboard handoff.
- Final medium meadow: 26.44 ms (+4.44 ms, 20.2% against baseline), same camera/resolution, single runs. All 20 tests pass. Scope performance unmeasured.
