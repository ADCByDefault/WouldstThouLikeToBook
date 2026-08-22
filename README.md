# <center>OPERATING SYSTEMS PROJECT REPORT <br><br> WouldstThouLikeToBook<br><br>

<div align="center">
<br>
  <img src="https://upload.wikimedia.org/wikipedia/it/c/c1/Logo_Unifi_2025.png?utm_source=it.wikipedia.org&utm_campaign=index&utm_content=original" alt="Universitas Florentina Studiorum" width="400" style="filter: drop-shadow(0 0 5px rgba(0,0,0,0.5)) invert(.9)"/>
  
  <br><br>
  
  **Author:** Swaran Singh  
  **Student ID:** 7159864
</div>

<br><br>

## 1. Project Overview

This project is a distributed **Client-Server Booking System** built entirely in C using POSIX sockets (TCP). The main goal of the application is to allow the concurrent booking of shared university resources (such as classrooms, teaching labs, or meeting rooms). 

It is designed to ensure two fundamental requirements of modern information systems:
1. **Atomic Data Consistency:** Preventing data corruption when multiple users try to book the same room at the exact same time.
2. **User Privacy:** Ensuring users can only see their own private information and bookings.

### 1.1 User Roles
The system implements strict access control with three distinct authorization levels:
* **GUEST (Unauthenticated User):** Can register for a new account, log in, and view the public list of all available rooms.
* **USER (Standard Authenticated User):** Can submit new booking requests for a specific resource. Users specify the date, start time, and the number of 1-hour slots they need. For privacy reasons, they can only consult their own personal bookings.
* **SUPERUSER (Administrator):** Has full control over the system. Can create new rooms, view the global list of all bookings, approve or reject pending requests, and manually force a record's state if necessary.


## 2. Technical Features (Under the Hood)
*This system uses advanced operating system concepts to ensure reliability and speed.*

* **Multiprocess Architecture:** The server uses a "Forking" model. When a new client connects, the main server spawns a dedicated child process (`server_child`) using `fork()` and `execl()`. This ensures that if one user has a slow connection, it doesn't block everyone else.
* **Network Protocol:** Communication happens via a custom TCP application-layer protocol. Every message has an 8-byte header (Action Code + Payload Size) followed by the actual data.
* **Cross-Platform Safety:** All numbers sent over the network are converted to standard Network Byte Order (Big-Endian) to ensure the system works perfectly even if the client and server run on computers with different architectures.
* **Concurrency Control:** To prevent two child processes from writing to the database at the same time and corrupting the files, the system uses file-system level locking (`fcntl()`). It applies "Shared Read Locks" for viewing data and "Exclusive Write Locks" for modifying data.


## 3. User Manual & Step-by-Step Setup Guide
*Follow these instructions to compile and run the Booking System on your computer. You do not need advanced technical knowledge.*

### 3.1 What You Need (Prerequisites)
Since this project uses low-level POSIX libraries, you must run it in a Linux environment. 
* **Operating System:** Linux (e.g., Ubuntu 24). If you are on Windows, you can use **WSL** (Windows Subsystem for Linux).
* **Tools:** `gcc` (version 13.3 or similar) and `make`.

### 3.2 Compilation and Initial Setup
Open your Linux terminal, navigate to the folder containing the project files, and follow these exact steps:

**Step 1: Compile the Code**
Simply type the following command and press Enter:

```bash
make
```
This reads the Makefile and automatically compiles the server, the client, and all utility programs.

**Step 2: Populate the Database (Crucial for First Run)**
If you start the server right now, the database will be completely empty (no rooms, no users execpt SUPERUSER admin with password admin). To test the system properly, generate some dummy data by typing:

```bash
make populate
./populate
```

Have a look at the `populate.c` file to see what data is being generated. You can modify it if you want to create your own rooms or users.

### 3.3 How to Run the Application
The Client and the Server are separate programs. You must run them in two separate terminal windows.

**First: Start the Server**
Open your first terminal window, make sure you are in the project folder, and run:

```bash
./server
```

(The server will start silently and listen for connections. Leave this window open in the background.)

**Second: Start the Client**
Open a new terminal window, navigate to the project folder, and run:

```bash
./client
```

### 3.4 Interacting with the Program
The Client features a very intuitive, text-based terminal menu.

When you launch `./client`, it will automatically read the `.client_settings` file and connect to the local server.

The screen will display a numbered list of available actions (e.g., 100 to Login, 101 to Signup).

To choose an action: Just type the corresponding number (`Opcode`) and press Enter.

*(Note: The client is smart enough to check if you have the right permissions for a command before even sending it to the server!)*

### 3.5 Troubleshooting & importing data
If you ever want to completely reset the system and wipe all data (users, rooms, bookings), go to your terminal and type:

```bash
make clean-data
```

If you want to delete all the compiled program files and start fresh, type:

```bash
make clean
```
If you want to import or exoprt data: just copy and paste `rooms.dat` `users.dat` `bookings.dat` files.