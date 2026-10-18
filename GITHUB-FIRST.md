# GitHub-first build policy

FAST: code build plus deterministic safety contracts, no long-lived package.
CANDIDATE: manual dual-architecture build and exact SHA-bound component.
RELEASE: requires explicit approval and real runtime acceptance; never rebuild.
The `qualify` check always reports for pull requests, including documentation-only changes.
Documentation-only changes skip compilation; changes to source, scripts, workflows or unknown refs run full validation.
