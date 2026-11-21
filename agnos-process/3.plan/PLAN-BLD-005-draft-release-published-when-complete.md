# PLAN-BLD-005: Draft Release, Published by the Last Workflow

## Overview

Makes the nine publishing workflows (six preprod, three prod) publish into one release that is created as
a draft and published by the workflow that attaches the last archive, so that it works with GitHub's
release immutability (RQ-BLD-016, ADR-BLD-006).

## References
- **Requirements**: RQ-BLD-016 (`process/1.requirements/RQ-BLD-build-tooling.md`); RQ-BLD-007, RQ-BLD-010
- **ADRs**: ADR-BLD-006 (DEC-BLD-029, DEC-BLD-030); ADR-BLD-003 (the publishing step it amends)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-BLD-013: Publish through a draft, completed by the last workflow
- **Tier**: L
- **Status**: In Progress
- **Description**: Move the logic of `publish-deployment` into `publish.sh` (draft, oldest draft wins, upload by id, publish at the expected count), pass `expected-assets` from the generator, regenerate the workflows, and test the script against a stand-in `gh` and `curl`.
- **Requirement refs**: RQ-BLD-016, RQ-BLD-007, RQ-BLD-008
- **ADR refs**: ADR-BLD-006 (DEC-BLD-029, DEC-BLD-030)
- **Acceptance Criteria** (Gherkin): *Given* the stand-in and six runs of the script with the same tag, *When* they run one after the other, *Then* the release is a draft after five and published with six assets after the sixth. *Given* a competing draft of the same tag with a lower id created between the lookup and the creation, *When* the script continues, *Then* its own draft is deleted and its archive is on the older one. *Given* a published release, *When* the script runs, *Then* it exits non-zero and says the release is published. *Given* an archive of the same name already on the draft, *When* the script runs again, *Then* it holds one archive of that name. *Given* a `PATCH` that fails because a concurrent workflow published, *When* the release is published, *Then* the script succeeds. *Given* the generator then `--check`, *When* the nine publishing workflows are read, *Then* they carry `expected-assets` 6 (preprod) or 3 (prod), and the canary ones are unchanged. *Given* the branch on `dev`, *When* the six preprod workflows run, *Then* one published immutable release holds six assets.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `python -I -m unittest discover -s juce/tests/tools -p test_publish_deployment.py`: 12 tests OK (9 run `publish.sh` against the stand-in `gh`/`curl`: draft until the sixth archive then published, prerelease/latest flags, competing older draft kept, published release refused, same-name archive replaced, concurrent publish tolerated, canary refused; 3 read the generator). Three alterations of the script (loser keeps its draft, publish one archive early, create published) each failed tests, restored identical. `generate_workflows.py` then `--check`: 15 up to date; the 9 publishing workflows carry `expected-assets` 6 (preprod) or 3 (prod), the 6 canary ones are unchanged. Not verified: the calls to GitHub itself (`gh api` flags, `curl` upload to uploads.github.com, publication of a draft under immutability, tag created at the commit) and the last Gherkin clause: they run only on `dev`, in CI. `tidy.py` is not on this branch and no C++ changed.
- **Assumptions**: The repository setting stays on. XplorerEditor is not changed. The assets are counted by number, not by name.
