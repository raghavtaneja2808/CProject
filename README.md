# ClassWatch

**A TCP client–server system in C that tells a teacher which students are actually present in an online class.**

---

## 1. Problem Statement

In online classes, a teacher looks at a grid of names and black camera boxes. Students often join the call, turn their camera off and walk away, fall asleep or switch to something else. The teacher has no reliable way to know who is really attending, and calling out names one by one wastes class time.

ClassWatch gives the teacher a live view of who is present and active. It warns inactive students, removes those who stay inactive, and produces an attendance report at the end of class.

## 2. Why Not Just Check the Camera?

The first idea was to read Google Meet's camera status and remove students whose camera stayed off for more than 10 minutes. After investigating, that approach was rejected:

| Approach | Problem |
|---|---|
| Intercept Google Meet's network traffic | Meet's traffic is encrypted, its message format is private and changes without notice, and tampering with it breaks Google's terms of service. |
| Use an official Meet API | No public API reports a participant's camera status in real time or allows removing them. |
| Detect "camera on" | A camera that is on does not prove presence (it can point at the ceiling). Many students also have no camera or a slow connection. |

So ClassWatch measures **presence**, not the camera:

1. **Activity tracking:** the student's client asks the operating system how many seconds have passed since the last keyboard or mouse input, and reports that number to the teacher.
2. **Code-based attention checks:** the teacher's server generates a random code that is shown **only inside the class** (spoken aloud or shown on screen in Meet). Students must type it into ClassWatch within a time limit. Only someone following the class can know the code, so it does not matter which device ClassWatch runs on.

ClassWatch runs **alongside** any video-call tool (Google Meet, Zoom, Teams). It does not depend on any of them.

## 3. Goals

- Let a teacher see, live, which students are connected and how long each has been inactive.
- Automatically **warn** a student after a period of inactivity, and **remove** them if it continues.
- Let the teacher run **attention checks** that only students following the class can pass.
- Produce an **attendance report** with join time, leave time and active minutes for each student.
- Learn and demonstrate low-level networking in C: TCP sockets, a custom text protocol, and serving many clients at once with `select()`.

### Non-goals

- Reading or recording students' video, audio, screen or keystrokes. ClassWatch only ever sends *how many seconds* a student has been idle.
- Replacing the video-call software.
- Being impossible to cheat. See [Limitations](#8-limitations).

## 4. Specifications

### 4.1 Components

| Program | Who runs it | Role |
|---|---|---|
| `server` | Teacher | Waits for students, tracks each one, applies the rules, accepts teacher commands, writes the report. |
| `client` | Each student | Connects to the teacher, sends a heartbeat every 5 seconds, shows warnings and attention checks. |

### 4.2 Rules (configurable)

| Situation | Default | Action |
|---|---|---|
| No heartbeat received | 20 seconds | Student is marked as disconnected. |
| Inactive (no keyboard or mouse input) | 8 minutes | Warning sent to the student; alert shown to the teacher. |
| Inactive | 10 minutes | Student is removed from the class. |
| Attention check not answered correctly | 60 seconds | Student is removed from the class. |

For demonstrations, these times can be shortened (for example, 30 seconds instead of 10 minutes).

### 4.3 Teacher Commands

| Command | Effect |
|---|---|
| `list` | Show every connected student: roll number, name, status, idle time. |
| `check` | Start an attention check: generate a code and send the check to all students. |
| `kick <roll>` | Remove a student manually. |
| `report` | Save the attendance report to a CSV file. |
| `quit` | End the class, save the report and disconnect everyone. |

### 4.4 Protocol

Client and server talk over **TCP** using a simple text protocol: one message per line, ending in `\n`. Text was chosen over binary so messages are easy to debug and can be read directly in tools like Wireshark or `nc`.

| Direction | Message | Meaning |
|---|---|---|
| Student → Teacher | `HELLO <class_code> <roll> <name>` | Join the class. |
| Student → Teacher | `BEAT <idle_seconds>` | "I'm still here; I've been idle this long." |
| Student → Teacher | `ANSWER <code>` | Answer to an attention check. |
| Student → Teacher | `BYE` | Leaving the class. |
| Teacher → Student | `WELCOME <message>` | Joined successfully. |
| Teacher → Student | `DENIED <reason>` | Join refused (wrong class code, duplicate roll number). |
| Teacher → Student | `CHECK <seconds>` | Attention check: enter the code from class. |
| Teacher → Student | `WARN <message>` | Inactivity warning. |
| Teacher → Student | `KICK <reason>` | Removed from the class. |

## 5. Design

### 5.1 Architecture

```
  Student laptops                              Teacher laptop
 ┌──────────────┐                         ┌───────────────────────┐
 │ client       │ ── HELLO / BEAT ──────▶ │ server                │
 │              │                         │                       │
 │ reports idle │ ◀── CHECK / WARN ────── │ - tracks every student│
 │ time, shows  │                         │ - runs the timers     │
 │ checks       │ ── ANSWER ────────────▶ │ - teacher commands    │
 │              │ ◀── KICK ────────────── │ - attendance report   │
 └──────────────┘                         └───────────────────────┘
        ×N                 TCP, port 5050
```

### 5.2 Connection Setup

```
TEACHER (server)                     STUDENT (client)
socket()   create a socket           socket()   create a socket
bind()     claim port 5050
listen()   start waiting
accept()   take a new student  ◀──── connect()  dial teacher's IP and port
recv()     read messages       ◀──── send()     send HELLO, BEAT, ...
close()                              close()
```

### 5.3 Serving Many Students: `select()`

`accept()` and `recv()` **block**: they wait until something happens. A simple server waiting on one student would ignore everyone else. ClassWatch uses `select()`, which watches all sockets at once and reports which ones have data ready. The server then handles only those, so no single student can freeze the class.

```
loop forever:
    select() on: the listening socket + every student socket + the keyboard
    if the listening socket is ready  → accept() the new student
    for each ready student socket     → recv() and handle their message
    if the keyboard is ready          → run the teacher's command
    once per second                   → apply the rules (warn / kick / timeouts)
```

### 5.4 Data Structures

Each connected student is stored in a fixed-size array of structs:

```c
struct Student {
    int    sock;           /* this student's socket */
    int    roll;
    char   name[32];
    time_t joined_at;
    time_t last_heartbeat;
    long   idle_seconds;   /* latest value reported by the client */
    int    warned;         /* already warned for this idle period? */
    long   active_seconds; /* for the attendance report */
};
```

### 5.5 Preventing Cheating

| Attempt | Defence |
|---|---|
| Closing ClassWatch | Heartbeats stop, so the student is marked as disconnected. |
| Joining from outside the class | A class code is required to join. |
| Joining twice or as another roll number | Only one connection per roll number. |
| Wiggling the mouse to look active | Attention checks require a code revealed only in class. |

## 6. Project Status

- [x] Step 1: TCP connection: the server accepts a student and receives `HELLO`
- [ ] Step 2: Many students at once with `select()`
- [ ] Step 3: Heartbeats, inactivity warning and automatic removal
- [ ] Step 4: Idle-time detection and code-based attention checks
- [ ] Step 5: Teacher commands, attendance report, Windows support

## 7. Building and Running

### Prerequisites

- A C compiler (`gcc` or `clang`) and `make`
- macOS or Linux (Windows support is planned)

### Build

```bash
make
```

This creates two programs: `server` and `client`. To remove them, run `make clean`.

### Run

In one terminal, start the teacher's server:

```bash
./server
```

In another terminal, start a student:

```bash
./client
```

### Example

```
$ ./server
Bound to port 5050
Waiting for students on port 5050
A student connected
Student says HELLO 21 Rahul
```

You can also test the server without the client, using `nc` (netcat):

```bash
nc 127.0.0.1 5050
HELLO 21 Rahul
```

## 8. Limitations

- **Proxy attendance:** if another person attends on a student's behalf, ClassWatch cannot tell. This is the same limitation as proxy attendance in a physical classroom. The teacher can compare the ClassWatch list with the video call's participant list.
- **Unencrypted traffic:** messages are plain text over TCP. They contain no sensitive data (only roll numbers, names and idle times), but they could be read by someone on the same network.
- **Same network:** the teacher's laptop must be reachable from the students' laptops (for example, the same Wi-Fi or hotspot).

## 9. Future Work

- Optional camera-in-use detection on Windows and Linux.
- Encrypted connections (TLS).
- A web dashboard for the teacher.

## 10. Project Structure

```
.
├── src/
│   ├── server.c      teacher's program
│   └── client.c      student's program
├── include/          header files
├── Makefile
├── README.md
└── LICENSE
```

## 11. License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
