# Attendly – IoT-Based Smart Attendance System

## About the Project

Attendly is an IoT-based smart attendance system designed to automate student attendance recording using RFID technology and an ESP32 microcontroller.

The system aims to reduce manual attendance-taking processes, minimize recording errors and improve attendance management through cloud-based data storage and automation.

## Features

- RFID-based student identification.
- PIR motion detection to activate the system.
- Automated attendance recording.
- Firebase Realtime Database integration.
- Google Sheets attendance dashboard.
- Automated late and absent status monitoring.
- Email notifications for students.

## Technologies Used

| Component | Technology |
|---|---|
| Microcontroller | ESP32 |
| Programming Language | Arduino C++ |
| RFID | RC522 |
| Motion Sensor | PIR |
| Database | Firebase Realtime Database |
| Automation | Google Apps Script |
| Dashboard | Google Sheets |
| Display | 16x2 LCD I2C |

## System Workflow

1. PIR sensor detects approaching movement.
2. ESP32 activates the attendance system.
3. Student scans their RFID card.
4. ESP32 verifies the student's UID.
5. Attendance information is uploaded to Firebase.
6. Google Apps Script processes attendance records.
7. Attendance status is updated in Google Sheets.
8. Email notifications are sent for late or absent students.

## Project Objectives

- To develop an automated student attendance system.
- To integrate RFID and IoT technologies.
- To store attendance records digitally.
- To improve attendance monitoring and management.

## Project Status

Developed as an academic group project.

## Project Team

**Group:** Syntax Error
