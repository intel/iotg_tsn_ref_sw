_The PR review is to check for sustainability and correctness.  Sustainability is actually more business critical as correctness is largely tested into the code over time.   Its useful to keep in mind that SW often outlives the HW it was written for and engineers move from job to job so it is critical that code developed for Intel be supportable across many years.  It is up to the submitter and reviewer to look at the code from a perspective of what if we have to debug this 3 years from now after the author is no longer available and defect databases have been lost.  Yes, that happens all the time when we are working with time scales of more than 2 years.  When reviewing your code it is important to look at it from this perspective._

Author Mandatory (to be filled by PR Author/Submitter)
------------------------------------------------------
- Developer who submits the Pull Request for merge is required to mark the checklist below as applicable for the PR changes submitted.
- Those checklist items which are not marked are considered as not applicable for the PR change.
- Items marked with an asterisk suffix are mandatory items to check and if not marked will be treated as non-compliant pull requests by the developers for Inner Source Development Model (ISDM) compliance

### PULL DESCRIPTION (to be filled by PR Author/Submitter)
_Provide a 1-2 line brief overview of the changes submitted through the Pull Request..._
- Platform Name:
- Component Name:
- Description:
- Dependencies (Optional): Any dependency from submodule or core repo
- GIO Execution Summary Link(Optional): or attach the execution logs

### REFERENCES (to be filled by PR Author/Submitter)
Reference URL for issue tracking (JIRA/HSD/Github): **\<URL to be filled>**
- [ ] **_Added label to the Pull Request following the template: ISDM\_\<Complexity>\*_** \
	Note-1: Depending on complexity of code changes, use the suitable word for complexity: Low/Medium/High \
	Example: PR for Slim boot loader project with medium complexity can have the label as: ISDM_Medium
- [ ] Added label to the Pull Request for easier discoverability and search

### CODE MAINTAINABILITY (to be filled by PR Author/Submitter)
- [ ] **_Commit Message meets guidelines as indicated in the URL\*_**
	- https://github.com/edgexfoundry/edgex-go/blob/main/.github/Contributing.md
- [ ] **_Every commit is a single defect fix and does not mix feature addition or changes\*_**
- [ ] Added required new tests relevant to the changes
	- [ ] PR contains URL links to functional tests executed with the new tests
- [ ] Updated Documentation as relevant to the changes
- [ ] Updated Build steps/commands changes as relevant - (**NA for STAR**)
- [ ] PR change contains code related to security - (**NA for STAR**)
- [ ] PR introduces changes that breaks compatibility with other modules (If YES, please provide description)
- [ ] Specific instructions or information for code reviewers (If any):

### UPSTREAM EXPECTATIONS (**NA for STAR**)
- [ ] Commit comment uses "imperative mood" ([See line 89-93 of Documentation/process/submitting-patches.rs](https://www.kernel.org/doc/html/latest/process/submitting-patches.html#describe-your-changes))
- [ ] PR does not break bisect
- [ ] PR does not break the compilation of other ISA's (i386, x86_64, AARCH64, ARM, PPC, RISCV, etc.)
- [ ] Kconfig's are well designed
- [ ] Signed-off-by and Reviewed-by tags are correctly formatted

Maintainer Mandatory (to be filled by PR Reviewer/Approving Maintainer)
-----------------------------------------------------------------------
- Maintainer who approves the Pull Request for merge is required to mark the checklist below as appropriate for the PR change reviewed as key proof of attestation indicating reasons for merge.
- Those checklist items which are not marked are considered as not applicable for the PR change.
- Items marked with an asterisk suffix are mandatory items to check and if not marked will be treated as non-compliant pull requests by the maintainers for ISDM compliance.

### QUALITY CHECKS (to be filled by PR Reviewer/Approving Maintainer)
- [ ] Architectural and Design Fit
- [ ] **_Quality of code (At least one should be checked as applicable)\*_**
	- [ ] Commit Message meets guidelines
	- [ ] PR changes adhere to industry practices and standards
	- [ ] Upstream expectations are met
	- [ ] Adopted domain specific coding standards ([SBL Coding Standards](https://github.com/tianocore-docs/Docs/raw/master/Specifications/CCS_2_1_Draft.pdf))
	- [ ] Error and exception code paths implemented correctly
	- [ ] Code reviewed for domain or language specific anti-patterns
	- [ ] Code is adequately commented
	- [ ] Code copyright is correct
	- [ ] Tracing output are minimized and logic
	- [ ] Confusing logic is explained in comments
	- [ ] Commit comment can be used to design a new test case for the changes
- [ ] **_Test coverage shows adequate coverage with required CI functional tests pass on all supported platforms\*_**
- [ ] **_Static code scan report shows zero critical issues\*_**

### CODE REVIEW IMPACT (Optional - need to discuss with ISDM forum)
- Summary of Defects Detected in Code Review: **\<%P1*xx,P2*xx,P3*xx,P4*xx%>** \
Note P1/P2/P3/P4 denotes severity of defects found (Showstopper/High/Medium/Low) and xx denotes number of defects found

### SECURITY CHECKS (**NA for STAR**)
Please check if your PR fulfills the following requirements:

- [ ] Avoid checks that rely on the result of undefined C behavior.
Instead, clarify the check condition using defined behaviors. Example: `if (x > INT_MAX-100)` instead of `if (x + 100 < x)`
- [ ] Use safe arithmetic to avoid integer overflow.
- [ ] Follow best practices when handling primitive data types
- [ ] Follow security best practices when dealing with pointers
- [ ] Avoid uncontrolled format strings
- [ ] Prevent buffer overflow/underflow
- [ ] Avoid unsafe functions
- [ ] Configure minimal permissions when opening pipes and ports
- [ ] Check contents within input structures are valid before use
- [ ] All forms of input validated
- [ ] Avoid inter-process race conditions
- [ ] Error and exception handling implemented
- [ ] Use keywords and #pragma to constrain gcc compiler optimization
- [ ] Prevent compiler from optimizing out security checks

# _Code must act as a teacher for future developers_
