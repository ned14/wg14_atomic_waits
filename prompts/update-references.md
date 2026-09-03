Read the papers listed at https://www.open-std.org/jtc1/sc22/wg14/www/wg14_document_log.htm.

Find new published revisions of this paper, and add them to the
appropriate places in `proposed-wording.md` and `description.md`. Update the Revision History
in `description.md` to reflect changes since the last published revision by examining the
delta of changes in git history -- be very terse in each item you add to the revision history,
matching the style of the existing items in the revision history.

If there is a newer "Working Draft" paper, that needs to be set as the new "base for diff".
`proposed-wording.md` must be rebased onto the new Working Draft paper so it cleanly applies.
Update `prompts/compare-to-standard.md` to refer to the new Working Draft paper. Place a copy
of the new working draft into the `reference` folder.

If there are new "Meeting Minutes" paper, those must be scanned for references to this paper
and the "Relevant polls" section in `proposed-wording.md` updated.