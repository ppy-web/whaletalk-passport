# Publisher API

The bundled script is the preferred interface. Use these details only to diagnose a failure.

## Authorization

### One-time official prompt

- The signed-in website calls `POST /api/agent/publish-grants` with session cookies and CSRF protection. An empty body or `{}` requests an ordinary `scope: create_once` grant with `parentProjectId: null`; `{ "parentProjectId": <id> }` (former Remix grants) now answers 410 because Remix is retired; `{ "projectId": 42 }` requests `scope: update_once` pinned to owned community play 42. It returns `authorizationCode`, `expiresAt`, `scope`, `projectId` and `parentProjectId` with `Cache-Control: no-store`. Unknown fields, malformed IDs, or a body containing both IDs are rejected, never silently treated as ordinary creation.
- `authorize --code-stdin` exchanges that code through `POST /api/agent/publish-token` with `authorization_code`. No browser approval/polling is needed.
- The high-entropy code is hashed at rest, valid for 24 hours, and atomically exchangeable once. The token expires at that same deadline. Revoking all AI access or resetting the account password also revokes pending grants.
- A create token allows ONE new project and cannot update. Submissions must omit `parent_project_id`. An update token allows ONE revision of the specified project and cannot create or change other plays. `GET /api/agent/me` returns `scope`, `projectId` and `parentProjectId`; `submit --auto` checks all applicable IDs against the requested operation.
- Remix is retired (#546): `POST /api/agent/remix-bind`, `GET /api/agent/remix-drafts` and the other remix endpoints answer 410. `GET /api/agent/me` keeps `parentProjectId`, `remixDraftProjectId` and `remixAccess` for older clients; they are null or closed.
- `GET /api/agent/projects` lists only the grant's project and adds `submissionResult`, initially null and then the exact committed receipt. Identical retries with the same one-time token return this result without additional projects, revisions or notifications. Different content after consumption returns 409. An expired/revoked/exchanged code returns 400; request a new official prompt if the exchanged token is unavailable.
- Update grants record the current base revision and version. Draft/pending revisions block creation of the grant; at submission the server rechecks for conflicts and changed base versions under a write lock. Conflicts return 409 without replacing work. Do not withdraw/delete a version or issue another grant as a retry workaround. Hidden plays stay hidden until explicitly republished through the existing review/visibility workflow.

### Existing browser authorization (compatible)

- `POST /api/agent/device-code` creates a thirty-minute browser authorization request; honor the returned `expiresIn`.
- Open `verificationUriComplete`; the creator signs in or registers and approves the displayed code.
- Poll `POST /api/agent/token` with `device_code`. HTTP 428 means authorization is still pending.
- The returned bearer token is scoped to creator submissions, expires after 30 days, and can be revoked from the creator page.

## Creator operations

Bearer authentication uses `Authorization: Bearer <token>`.

- `GET /api/agent/me` verifies the authorization.
- `GET /api/agent/projects` lists the creator's latest project revisions.
- `POST /api/agent/submissions` creates a project.
- `POST /api/agent/submissions/{project_id}/resubmit` submits a revision.

Submission endpoints accept multipart fields `title_zh`, `description_zh`, optional `title_en`, `description_en`, and `github_url`, plus **required** `cover` and `firmware`. Omit `category`, `tag_keys` and discovery flags for firmware submissions: the server AI assigns them asynchronously. Never ask users to classify their upload. The cover must be an exact portrait 3:4 image. Optional repeated `images` fields contain up to four extra images, excluding the separate cover (maximum five total); the CLI assembles this from `--cover` and up to four `--image` options. No runtime screenshot or serial receipt is required. The historical optional `github_url` field name accepts GitHub, Gitee, GitLab, Codeberg, or another public HTTPS Git repository page. Server validation remains authoritative.

Do not send `allow_remix` or `parent_project_id`. Remix is retired: `allow_remix=true` or any parent ID is rejected with 410. A public `github_url` is still shown as the play's source.

Do not retry a rejected mutation automatically. Show the response to the creator and resolve the cause first.

## Firmware content format 2

Multipart create/resubmit accepts `content_format=2`, `instructions_zh`, `instructions_en`,
`changelog_zh`, `changelog_en`. Instructions allow 8000 characters per language; release notes
allow 4000. Format 2 requires Chinese instructions and, for updates, Chinese release notes.
Use complete valid text, not placeholders. These are version fields and publish with the firmware.

Legacy requests omit `content_format` (format 1): omitted instructions retain prior values on
updates; omitted release notes start empty. An explicit empty text field clears that language.
Identical grant retries include these fields in the request hash. Draft PATCH retains omitted
fields, including that draft's release notes; draft creation copies instructions but resets notes.
New browser draft submission sends JSON `contentFormat: 2`; old clients remain supported.

Read `instructions` and `changelog` bilingual objects in project/revision responses. Agent
projects also include `publishedRevision` for comparison with the public baseline. Public detail
responses include `changelogHistory` with only previously published revisions, never drafts or
rejected content. Missing fields in an older server mean unavailable; update the server before
using the format 2 publisher rather than assuming it stored unsupported fields.

## Activity firmware

Event-bound one-time grants return `eventId` from token exchange and `/api/agent/me`. Send that exact ID as multipart `event_id`. The server checks check-in, submission availability and unchanged project/event ownership before saving. Missing or mismatched IDs fail rather than creating an ordinary submission. Event grants only support firmware.
