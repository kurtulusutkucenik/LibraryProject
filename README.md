# 📚 Library Management System

A secure, terminal-based library management application featuring robust file encryption, a two-level user authentication system, and real-time transaction logging.

---

## 📋 Table of Contents

- [Features](#features)
- [Project Structure](#project-structure)
- [Security Architecture](#security-architecture)
- [Build and Run](#build-and-run)
- [Initial Setup](#initial-setup)
- [User Guide](#user-guide)
- [Data Files](#data-files)
- [Technical Details](#technical-details)

---

## Features

### 📖 Book Management
* **CRUD Operations:** Add, delete, and search books by ID, title, or author.
* **Fast Indexing:** Powered by a Binary Search Tree (BST) for high-performance lookup.
* **Flexible Sorting:** Sort dynamically by Alphabetical order, ID, or Genre.
* **Status Tracking:** Real-time tracking of book states (`On Shelf` / `Borrowed` / `Damaged`).
* **Fault Tolerance:** Supports Multi-level Undo (up to the last 10 operations).

### 👥 Member Management
* **ID Validation:** Government ID (TCKN) validation during member registration.
* **Secure Authentication:** Passwords are fully protected using `SHA-256` + `Salt` + `Pepper`.
* **Triple-Factor Verification:** Member login requires a precise match of TCKN, Full Name, and Password.

### 🔄 Loan & Fine System
* **Transactions:** Seamless checkout (borrowing) and check-in (return) workflows.
* **Automated Fines:** Daily rate calculations for overdue items.
* **Monitoring:** Admin overview of all active loans.

### 🖥️ Workspace Reservation
* **Capacity:** 20 dedicated study zones (`T1`–`T20`).
* **Fair-Use Policy:** Strict limit of one active reservation per member.
* **Role-Based Control:** Separate workspace access permissions for Admins and Members.
* **Memory Integrity:** Protected via canary value verification to detect buffer manipulation.

### 🔒 Security & Audit Logs
* **Isolated Logs:** Separate logging pipelines for Admin and Member activities.
* **Audit Trails:** Append-only text log files for tamper-evident history tracking.
* **Data-at-Rest Encryption:** All database files are fully encrypted with **AES-256-CTR**.
* **Integrity Checks:** Cryptographic verification using **SHA-256 MAC** tags.

---

## Project Structure


.
├── main.c           # Entry point, menu routing, and global state initialization
├── book.c/h         # Book database engine (BST, CRUD handlers, undo stack)
├── member.c/h       # Member subsystem (Linked lists, authentication logic)
├── loan.c/h         # Loan lifecycle, tracking, and fine calculation rules
├── workspace.c/h    # Workspace allocation, slots, and canary validation
├── log.c/h          # Circular log buffer and append-only audit files
├── config.c/h       # System environment, bootstrapping, and credential hashing
├── common.c/h       # I/O utilities, input sanitization, and crypto abstractions
├── aes.c/h          # Custom implementation of AES-256-CTR mode
├── sha256.c/h       # Custom implementation of the SHA-256 hashing algorithm
└── data/            # Runtime directory containing encrypted data stores

Security ArchitecturePassword Hashing PipelineUser passwords are never stored in plaintext. The system enforces the following pipeline:$$\text{SHA-256}(\text{password} + \text{salt} + \text{pepper}) \rightarrow \text{Hex String} \rightarrow \text{Database Storage}$$Salt: A unique, cryptographically secure 30-character string generated from /dev/urandom per user.Pepper: Read securely via the KUTUPHANE_PEPPER environment variable. Falls back to a compile-time string if not set.Timing Attack Prevention: Password verifications use a constant-time comparison helper (secure_compare()).💡 Production Notice: Always set your environment pepper string before deploying:

export KUTUPHANE_PEPPER="your_long_and_very_secure_secret_pepper_here"

#### Seed User Credentials (For Testing)
| Full Name | National ID (TC) | Password |
| :--- | :--- | :--- |
| Ahmet Yılmaz | 12345678901 | ahmet123 |
| Fatma Kaya | 23456789012 | fatma456 |
| Mehmet Demir | 34567890123 | mehmet789 |
| Ayşe Çelik | 45678901234 | ayse321 |
| Mustafa Şahin | 56789012345 | musta00 |

### Storage Encryption Layout
Every storage file written to disk utilizes a tightly managed binary layout:
[Magic Bytes (4B)] [Version (1B)] [IV (16B)] [Payload Size (8B)] [Ciphertext + SHA-256 MAC (32B)]

* **Algorithm:** AES-256 in Counter Mode (CTR).
* **IV Freshness:** A new Initialization Vector (IV) is pulled from /dev/urandom on every write cycle to completely neutralize key-stream reuse risks.
* **Data Integrity:** A SHA-256 MAC tag appended to the ciphertext ensures file modifications are caught during decryption.
* **Key Hierarchy:** The master kutuphane.cfg configuration file is encrypted with a hardcoded *bootstrap key*. All primary .db instances use a dynamic *runtime key* stored inside that config file.
* **Atomic Writes:** Saves utilize file swapping via rename() to eliminate partial file writes or corruption during power loss.

### Additional Hardening Measures
* **Safe Input Handling:** All console lines pass through a custom safe_read() bounded-length buffer constraint.
* **Sanitization:** String outputs fetched from files are forced into proper null-termination.
* **Anti-Brute Force:** Triggering 3 consecutive incorrect administrator logins shuts down the runtime automatically.

---

## Build and Run

### Prerequisites
* GCC or any C99-compliant compiler.
* A POSIX-compliant environment (Linux / macOS).
* make utility (Optional).

### Compilation
Compile directly using GCC:
```bash
gcc -o kutuphane \
    main.c book.c member.c loan.c workspace.c \
    log.c config.c common.c aes.c sha256.c \
    -Wall -Wextra -O2

OR 

make

./kutuphane


Initial SetupWhen the application launches for the first time, it checks for the existence of data/kutuphane.cfg. If missing, the Initialization Wizard kicks in:You will be prompted to supply and confirm a master Administrator password.The core framework automatically spins up a cryptographically random AES-256 operational runtime key.The system locks down kutuphane.cfg using the bootstrap key wrapper.Database instances are fully populated with standard demo datasets (40 starter books, 5 seed user accounts, and 20 empty desk allocations).User GuideMain MenuPlaintext╔════════════════════════════════════╗
║     LIBRARY MANAGEMENT SYSTEM      ║
╠════════════════════════════════════╣
║  1) Staff / Librarian Login        ║
║  2) Member Login                   ║
║  0) Exit Application               ║
╚════════════════════════════════════╝
🔑 Librarian (Admin) DashboardKeyOperation1Add new book item2Delete book item (Requires cross-verifying ID, Author, and Title)3Search books (Lookup by target ID or Author name)4Sort system index (Alphabetical / ID order / Category)5Register new user account6Terminate member registration7Review library roster (Sorted by National ID)sTarget-match a member profile8Audit active system-wide loans9Fetch the last 10 master admin system event logspModify admin credential profileuUndo the last catalog state changetMap all workspace availability slotsnList vacant workspace slotsfList reserved workspace slotsaAllocate/Reserve a workspace slotdClear/Checkout a workspace reservation0Commit state changes and terminate runtime safely👤 Member DashboardKeyOperation1Checkout/Borrow a book resource2Check-in/Return a book resource (Evaluates accrued fine balances)3Display catalog with custom sorting4Query book catalog by Title or Author5Update personal password profile6Retrieve personal activity logs (Last 10 actions)tInspect all workspace layoutsnLocate vacant workspace desksaRequest workspace slot reservationdTerminate/Surrender desk reservation0Save user transaction state and log out🛠️ Loan & Policy ParametersStandard Rental Period: 14 Calendar Days.Late Fee Accumulation Rate: 2.00 TL per day overdue.Damaged Items: Checking out items marked as damaged requires explicit acknowledgment.Data FilesAll files are maintained under the dedicated data/ subdirectory:File NameTarget ContentsStorage Statekutuphane.cfgMaster credential verification hash, AES runtime key valuesEncryptedbooks.dbComplete book indexing ledger (In-order BST sequence)Encryptedbooks.idxSecondary index tracking matrix for Book IDsEncryptedmembers.dbSystem profile entries for registered library membersEncryptedloans.dbReal-time active allocation recordsEncryptedlog_admin.dbCircular buffer for active admin tracking parametersEncryptedlog_user.dbCircular buffer for user event validation metricsEncryptedworkspace.dbTrack status fields for study layouts 1 to 20Encryptedaudit_admin.logPermanent historical admin actions (Append-only text layout)Plaintextaudit_user.logPermanent historical user actions (Append-only text layout)PlaintextTechnical DetailsArchitectural Layout & Core PrimitivesModuleData Structure SelectionDesign Rationalebook.cBinary Search Tree (BST)Provides swift $O(\log n)$ search performance for high-volume catalogs.member.cSingly Linked ListLow overhead; insertion sort profiles are sufficient for expected load bounds.loan.cSingly Linked ListOptimizes management for variable, highly temporary items.log.cCircular Buffer (Ring Buffer)Limits memory footprint while managing the fixed LOG_KEEP tracking state.workspace.cStatic Contiguous ArrayFixed resource bounds (MAX_WORKSPACE=20), ensuring deterministic $O(1)$ indexing.Cryptographic FoundationsZero External Dependencies: Both AES-256-CTR (aes.c) and SHA-256 (sha256.c) are implemented entirely from scratch in pure C.Key Separation Strategy: The configuration state is sealed using a primary bootstrap key; functional assets use a secondary, isolation-bounded operational runtime key.Memory Optimization & HardeningDynamic Safe Sizing: The book collection scales dynamically in heap memory space, utilizing deep recursive validation safety boundaries up to BST_STACK_MAX = 8192.Null Check Paradigm: Every single allocation request through malloc is bounded by strict error routing constraints to prevent null-pointer failures.Tainted Input Defense: File streams fetched from storage pass through data-type mapping boundaries to block memory exploitation attempts.
