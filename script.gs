function runDailyAttendanceSystem() {
 
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const attendanceSheet = ss.getSheetByName("Attendance");
  const studentsSheet = ss.getSheetByName("Students");
  const today = Utilities.formatDate(new Date(), "GMT+8", "yyyy-MM-dd");
  const firebaseUrl = "";


  // 1. Fetch Class Start Time for Today
  const startTimeResponse = UrlFetchApp.fetch(firebaseUrl + "/classStart/" + today + "/startTime.json");
  const startTimeStr = JSON.parse(startTimeResponse.getContentText());
 
  if (!startTimeStr) {
    Logger.log("No class start time found for " + today + ". System waiting for ESP32 to start.");
    return;
  }


  // 2. Fetch Attendance Data from Firebase
  const attendanceResponse = UrlFetchApp.fetch(firebaseUrl + "/attendance/" + today + ".json");
  const firebaseData = JSON.parse(attendanceResponse.getContentText()) || {};


  // 3. Get Master Student List (A: UID | B: Name | C: Email)
  const masterData = studentsSheet.getDataRange().getValues();
 
  // 4. Time Calculations
  const now = new Date();
  const parts = startTimeStr.split(":");
  const classStart = new Date();
  classStart.setHours(parseInt(parts[0]), parseInt(parts[1]), parseInt(parts[2]));
  const tenMinMark = new Date(classStart.getTime() + 10 * 60 * 1000); // 10 Min Reminder
  const thirtyMinMark = new Date(classStart.getTime() + 30 * 60 * 1000); // 30 Min Absent Mark


  // 5. MAP CURRENT SHEET DATA (FIXED: ONLY MAPPING ROWS FOR TODAY)
  const currentAttendance = attendanceSheet.getDataRange().getValues();
  const uidMap = {};


  for (let j = 1; j < currentAttendance.length; j++) {
    let sheetUid = currentAttendance[j][1].toString().trim(); // Column B
    let rawDate = currentAttendance[j][2]; // Column C
   
    // Convert sheet date to string to compare with 'today'
    let sheetDateStr = "";
    if (rawDate instanceof Date) {
      sheetDateStr = Utilities.formatDate(rawDate, "GMT+8", "yyyy-MM-dd");
    } else {
      sheetDateStr = rawDate.toString().trim();
    }


    // Only "remember" this UID if it belongs to today's date
    if (sheetUid !== "" && sheetDateStr === today) {
      uidMap[sheetUid] = j + 1; // Store row index
    }
  }


  // 6. PROCESS STUDENTS FROM MASTER LIST
  for (let i = 1; i < masterData.length; i++) {
    const sUid   = masterData[i][0].toString().trim();
    const sName  = masterData[i][1].toString().trim();
    const sEmail = masterData[i][2].toString().trim();
    const sTelegramId = masterData[i][3] ? masterData[i][3].toString().trim() : null;
   
    // Check if student exists in Firebase data for today
    let isPresent = false;
    let scanTime = "--";
   
    // Matching by UID key in Firebase
    if (firebaseData[sUid]) {
      isPresent = true;
      scanTime = firebaseData[sUid].time;
    }


    let status = isPresent ? "Present" : "Absent";
    let rowIndex = uidMap[sUid]; // Get the row if it exists for TODAY


    // 7. SYNC LOGIC (APPEND OR UPDATE)
    if (!rowIndex) {
      // NEW RECORD FOR TODAY
      attendanceSheet.appendRow([sName, sUid, today, scanTime, status, ""]);
      rowIndex = attendanceSheet.getLastRow();
      uidMap[sUid] = rowIndex; // Prevent duplicates in current loop
    } else {
      // UPDATE EXISTING RECORD FOR TODAY
      attendanceSheet.getRange(rowIndex, 4).setValue(scanTime); // Time (D)
      attendanceSheet.getRange(rowIndex, 5).setValue(status);   // Status (E)
    }


    // 8. EMAIL NOTIFICATION LOGIC
    let mailFlag = attendanceSheet.getRange(rowIndex, 6).getValue().toString().trim();
   


    // If Scanned -> Send confirmation
    if (status === "Present" && mailFlag !== "SCANNED_MAIL_SENT") {
      MailApp.sendEmail(sEmail, "Attendance Confirmed", "Hi " + sName + ",\n\nYour attendance for " + today + " has been recorded at " + scanTime + ".");
      if (sTelegramId) {
        sendTelegramNotification(sTelegramId, "✅ <b>Attendance Confirmed</b>\nHi " + sName + ", your attendance was recorded at " + scanTime);
      }
      attendanceSheet.getRange(rowIndex, 6).setValue("SCANNED_MAIL_SENT");
    }
    // If 10 mins late and still absent -> Send Warning
    else if (status === "Absent" && now >= tenMinMark && now < thirtyMinMark && mailFlag === "") {
      MailApp.sendEmail(sEmail, "Attendance Reminder", "Hi " + sName + ",\n\nYou haven't scanned in yet. Class started at " + startTimeStr + ".");
      attendanceSheet.getRange(rowIndex, 6).setValue("REMINDER_SENT");
      if (sTelegramId) {
        sendTelegramNotification(sTelegramId, "⚠️ Reminder: You haven't scanned in yet! Class started at " + startTimeStr);
      }
    }
    // If 30 mins late -> Final Absent Mail
    else if (status === "Absent" && now >= thirtyMinMark &&
             mailFlag !== "ABSENT_MAIL_SENT" &&
             mailFlag !== "SCANNED_MAIL_SENT") {
      MailApp.sendEmail(sEmail, "Absent Notification", "Hi " + sName + ",\n\nYou have been marked Absent for today's class.");
      attendanceSheet.getRange(rowIndex, 6).setValue("ABSENT_MAIL_SENT");
      if (sTelegramId) {
        sendTelegramNotification(sTelegramId, "❌ " + sName + ", you have been marked Absent for today.");
      }
    }
  }
}






function uploadToFirebase() {
  const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  const rows = sheet.getDataRange().getValues();


  // Remove header row
  rows.shift();


  const baseUrl =
    "url";


  rows.forEach(row => {
    const [uid, name, email] = row;
    if (!uid) return; // skip empty rows


    const url = `${baseUrl}/${uid}.json`;


    const payload = {
      name: name,
      email: email,
     
    };


    const options = {
      method: "put", // PUT = use UID as key
      contentType: "application/json",
      payload: JSON.stringify(payload),
      muteHttpExceptions: true
    };


    const res = UrlFetchApp.fetch(url, options);
    Logger.log(res.getContentText());
  });
}


/**
 * Standalone helper to send Telegram messages
 * Does not interfere with MailApp
 */
function sendTelegramNotification(chatId, message) {
  const token = "token";
  const url = "https://api.telegram.org/bot" + token + "/sendMessage";
 
  const payload = {
    "chat_id": chatId,
    "text": message,
    "parse_mode": "HTML"
  };


  const options = {
    "method": "post",
    "contentType": "application/json",
    "payload": JSON.stringify(payload),
    "muteHttpExceptions": true
  };


  try {
    UrlFetchApp.fetch(url, options);
  } catch (e) {
    Logger.log("Telegram Error: " + e.message);
  }
}
