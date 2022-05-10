_The PR review is to check for sustainability and correctness.  Sustainability is actually more business critical as correctness is largely tested into the code over time.   Its useful to keep in mind that SW often outlives the HW it was written for and engineers move from job to job so it is critical that code developed for Intel be supportable across many years.  It is up to the submitter and reviewer to look at the code from a perspective of what if we have to debug this 3 years from now after the author is no longer available and defect databases have been lost.  Yes, that happens all the time when we are working with time scales of more than 2 years.  When reviewing your code it is important to look at it from this perspective._

Author Mandatory (to be filled by PR Author/Submitter)
------------------------------------------------------
- Developer who submits the Pull Request for merge is required to mark the checklist below as applicable for the PR changes submitted.
- Those checklist items which are not marked are considered as not applicable for the PR change.
- Items marked with an asterisk suffix are mandatory items to check and if not marked will be treated as non-compliant pull requests by the developers for Inner Source Development Model (ISDM) compliance

## PULL DESCRIPTION (to be filled by PR Author/Submitter)
_Provide a brief overview of the changes submitted through the Pull Request..._
- Platform Name: 
- HSD/RCR Link: **\<URL to be filled>**
- Description: 
- Test step/link: 

### CODE SUBMISSION CHECKLIST (to be filled by PR Author/Submitter)
- [ ] **_Every commit is a single defect fix and does not mix feature addition or changes\*_**
- [ ] Does the solution/implementation meet the requirement or fix the issue?
- [ ] Added required new tests relevant to the changes
	- [ ] PR contains URL links to functional tests executed with the new tests
- [ ] Updated Documentation as relevant to the changes
- [ ] Do you know who supposes to review your code?
- [ ] PR does not break the application compilation
- [ ] PR does not introduce new/regression issue
- [ ] PR has passed "Trigger Build Check"
- [ ] Specific instructions or information for code reviewers (If any):



Maintainer/Reviewer Mandatory (to be filled by PR Reviewer/Approving Maintainer)
-----------------------------------------------------------------------
- Maintainer/Reviewer should look for Structure, Style, Logic, Performance, Test coverage, Design, Readability (and maintainability), and Functionality.
- Maintainer/Reviewer is required to mark the checklist below as appropriate for the PR change reviewed as key proof of attestation indicating reasons for merge.
- Those checklist items which are not marked are considered as not applicable for the PR change.
- Items marked with an asterisk suffix are mandatory items to check and if not marked will be treated as non-compliant pull requests by the developers for Inner Source Development Model (ISDM) compliance

### CODE REVIEW CHECKLIST (to be filled by PR Reviewer/Approving Maintainer)
- [ ] Architectural and Design Fit
- [ ] **_Commit title/messages meets guidelines and contain the follow information\*_**
	- [ ] Requirement/reason for the code change
	- [ ] Code impact/expected result
	- [ ] Messages are understandable, the problem can be reproducible and the solution is testable
	- [ ] Signed-off-by included
- [ ] **_Quality of code (At least one should be checked as applicable)\*_**
	- [ ] PR changes adhere to industry practices and standards
	- [ ] Upstream expectations are met
	- [ ] Error and exception code paths implemented correctly
	- [ ] Code reviewed for domain or language specific anti-patterns
	- [ ] Code is adequately commented
	- [ ] Code copyright is correct
	- [ ] Tracing output are minimized and logic
	- [ ] Confusing logic is explained in comments
	- [ ] Code does not contain inclusive languages (For e.g., "Master", "Slave", "Blacklist" and "Whitelist")
- [ ] The code is understandable (Ask questions to clear your doubt, ask for test results or verify the code by yourself)
- [ ] The reviewers (Maintainer, domain expert, and dependency (tools and userspace) owner) been invited to review the code
- [ ] **_Test coverage shows adequate coverage with required CI functional tests pass on all supported platforms\*_**
- [ ] **_Static code scan report shows zero critical issues\*_**

# _Code must act as a teacher for future developers_
