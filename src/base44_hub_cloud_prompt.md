# Base44 prompt: Fanan Hub Cloud

Paste everything below the line into Base44's AI chat, in a **new app** (not the serial-generator site).

---

Create a backend-only app called **Fanan Hub Cloud**. It is the cloud for a desktop app (the Fanan Web Hub), which talks to this app through the Base44 API as an external app. No frontend pages are needed beyond Base44's defaults.

**Users and sign-in**
- Anyone can create an account with email and password (public registration).
- New accounts confirm their email with a one-time code sent by email (verify-otp), with resend available.
- Password reset by email must work.
- Do not require a captcha or bot check (Turnstile) on sign-in or registration: the desktop app signs in through the API and cannot show one.

**Entity: HubSong** (one record per song a user saves to the cloud)
- `name`: text, required (the song's name)
- `song`: long text (the song file, JSON as text)
- `media`: array of objects, each with `path` (text), `file_uri` (text), `size` (number), `modified` (number)
- `size_bytes`: number
- `app`: text

**Security for HubSong (row-level)**
- Create: any signed-in user.
- Read, update, delete: only the user who created the record (`created_by` equals the current user's email).
- No access at all for anonymous users. Users must never see or change each other's songs.

**Files**
- Signed-in users can upload private files with `UploadPrivateFile` and get temporary download links with `CreateFileSignedUrl`. Nothing is uploaded publicly.

---

**Afterwards:** open the app in the Base44 editor and copy its **app ID**, the part of the address between `/apps/` and `/editor/`. Send it to me, and I'll build it into the Hub.
