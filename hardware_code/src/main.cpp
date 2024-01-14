#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

#include <ESP_Mail_Client.h>

#define LED 2

/** The smtp host name e.g. smtp.gmail.com for GMail or smtp.office365.com for Outlook or smtp.mail.yahoo.com */
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

/* The sign in credentials */
#define AUTHOR_EMAIL "singerbodycount@gmail.com"
#define AUTHOR_PASSWORD "mecy kszs adgc eyfs"

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

/* Mailing List*/
const String RECIPIENT_EMAILS[] = {"AbuBakr.Siddique@singerbd.com",
                                   "Helal.Uddin@singerbd.com",
                                   "hakan.altinisik@arcelik.com",
                                   "TanvirAhmed.Fahim@singerbd.com",
                                   "saygin.yurtseven@arcelik.com",
                                   "ahsanul.kabir@singerbd.com",
                                   "moniruzzaman.mia@singerbd.com",
                                   "samiha.mustabin@singerbd.com",
                                   "redwan.ali@singerbd.com",
                                   "mumtarin.rahman@singerbd.com",
                                   "shahinur.islam@singerbd.com",
                                   "abul.basar@singerbd.com",
                                   "maliha.tasnim@singerbd.com",
                                   "meghla.roy@singerbd.com"};
const String RECIPIENT_NAMES[] = {"Abu Bakr Siddique",
                                  "Helal Uddin (GM Sir)",
                                  "Hakan Altinisik",
                                  "Tanvir Ahmed Fahim",
                                  "Saygin Yurtseven",
                                  "Mohammed Ahsanul Kabir",
                                  "Moniruzzaman Mia",
                                  "Samiha Mustabin Jaigirdar",
                                  "Redwan Ali Sheikh",
                                  "Mumtarin Rahman",
                                  "Shahinur Islam",
                                  "Abul Basar",
                                  "Maliha Tasnim",
                                  "Meghla Roy"};

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Declare the global Session_Config for user defined session credentials */
Session_Config config;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

/* Callback function to get the date in the correct format */
String dateFormat(int yearVal, int monthVal, int dayVal);

/* Callback function to get the time in the correct format */
String timeFormat(int hrVal, int minVal, int secVal);

/* Callback function to send the email */
// void sendMsg(String dateVal, String timeStart, String timeStop, int units, int totalUnits);
void sendMsg(String dateVal, String timeStart, String timeStop, int units, int totalUnits, int pastHrUnits, int currentHr, int currentMin, int currentSec);
size_t msgNo;

const int sensorPin = 25;
// Electrical Generator Room WiFi SSID: "UB-Wifi" "POCO C55", "SRF-GUEST", "BFM-Wifi"
// Electrical Generator Room WiFi password: "Ar@#Sbd7896" "Saad123456", "RS6S7LQH" (expires on Sunday - 24th September, 2023), "Singer@#bfm"

const char *ssid = "BFM-Wifi";
const char *password = "Singer@#bfm";
// const char *ssid = "POCO C55";
// const char *password = "Saad123456";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600 * 6;
const int daylightOffset_sec = 0;

// Date Variables
int todayDateCheck = -1;
int nowYear;
int nowMonth;
int nowDay;
String dateString;

// Time Variables
int lastHrVar = -1;
int nowHr;
int nowMin;
int nowSec;
String todayTimeStart;
String bodyRecTimeStart;
String bodyRecTimeStop;
unsigned long continuousTime = 0; // to store the Detection Time continuously

// const int contTimeInterval = 150; // 0.5s for the continuousTime var. to cross this
const int interval = 2000; // 2 seconds

struct tm timeinfo;

// Body Count variables
int hourlyBodyCtr, totalBodyCtr = 0, prevTotalBodyCt = 0;
// Debugging Purposes
int tempHourlyBodyCtr = 0;

// The Message:
String htmlMsg =
    "<div>"
    "<style>"
    "table {"
    "font-family: arial, sans-serif;"
    "border-collapse: collapse;"
    "width: 100%;"
    "}"

    "td, th {"
    "border: 1px solid #dddddd;"
    "text-align: left;"
    "padding: 8px;"
    "}"

    "tr:nth-child(even) {"
    "background-color: #dddddd;"
    "}"
    "</style>"
    "<p> [TODAY DATE]: Total bodies foamed upto now = 0 units</p>"
    "<table style=\"width:100%\">"
    "<tr>"
    "<th>Date</th>"
    "<th>Start from</th>"
    "<th>to</th>"
    "<th>units</th>"
    "</tr>"
    "</table>"
    "</div>";

bool isTodayEmailSent = 0;

// This message is to be sent if production is below 50 units
String htmlMsgProdShort = "";
void setup()
{
  Serial.begin(115200);
  pinMode(sensorPin, INPUT);
  pinMode(LED, OUTPUT);
  Serial.printf("Connecting to %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED, LOW);
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.printf("Connected to %s\n", ssid);
  digitalWrite(LED, HIGH);
  // Print local IP address and start web server
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); // show ip address when connected on serial monitor.

  /*  Set the network reconnection option */
  MailClient.networkReconnect(true);

  /** Enable the debug via Serial port
   * 0 for no debugging
   * 1 for basic level debugging
   *
   * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
   */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Set the session config */
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 6;
  config.time.day_light_offset = 0;

  // init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time. Please resolve it. Suggestion: Check the internet availability");
    return;
  }
  nowYear = timeinfo.tm_year + 1900; // Gives number of years since 1900, e.g. 2023 is +123 years from 1900
  nowMonth = timeinfo.tm_mon + 1;
  nowDay = timeinfo.tm_mday;
  todayDateCheck = nowDay;

  nowHr = timeinfo.tm_hour;
  nowMin = timeinfo.tm_min;
  nowSec = timeinfo.tm_sec;
  lastHrVar = nowHr;
  // lastHrVar = nowMin;
  todayTimeStart = timeFormat(nowHr, nowMin, nowSec);
  bodyRecTimeStop = todayTimeStart;
  dateString = dateFormat(nowYear, nowMonth, nowDay);
}

void loop()
{
  // Get the Wi-Fi Connection again
  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(LED, LOW);               // LED OFF when Wi-Fi disconnected
    while (WiFi.status() != WL_CONNECTED) // attempts to re-establish the Wi-Fi connection
    {
      Serial.printf("Connecting to %s\n", ssid);
      WiFi.begin(ssid, password);
    }
    digitalWrite(LED, HIGH); // Wi-Fi connection established
  }
  // gets the local time - necessary to iterate through in every instance of the loop
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time. Please resolve it. Suggestion: Check the internet availability");
    return;
  }
  else
  {
    bodyRecTimeStart = bodyRecTimeStop;
    ; // NULL Statement
    // Sensor detection code/function with body counting goes here
    if (digitalRead(sensorPin) == HIGH)
    {
      unsigned long prevMillis = millis();
      while (digitalRead(sensorPin) == HIGH)
      {
        // digitalWrite(LED,HIGH);
      }
      unsigned long currentMillis = millis();
      if ((currentMillis - prevMillis) != 0)
      {
        Serial.printf("Difference between Millis is %d\n", (currentMillis - prevMillis));
      }
      continuousTime += (currentMillis - prevMillis);
      if (continuousTime != 0)
      {
        Serial.printf("Continuous Time from Detection is %d ms\n", continuousTime);
      }
      if ((currentMillis - prevMillis >= interval))
      {
        if ((digitalRead(sensorPin) == LOW))
        {
          Serial.println("Body was detected by normal time difference");
          continuousTime = 0; // reset the continuousTime interval
          Serial.printf("Continuous Time variable should be reset, the value is %d\n", continuousTime);
          ++hourlyBodyCtr;
          ++totalBodyCtr;
        }
      }
      else if ((continuousTime >= interval))
      {
        if ((digitalRead(sensorPin) == LOW))
        {
          Serial.println("Body was detected by continuous Time");
          continuousTime = 0; // reset the continuousTime interval
          Serial.printf("Continuous Time variable should be reset, the value is %d\n", continuousTime);
          ++hourlyBodyCtr;
          ++totalBodyCtr;
        }
      }
    }
    // Get Current Time
    nowHr = timeinfo.tm_hour;
    nowMin = timeinfo.tm_min;
    nowSec = timeinfo.tm_sec;

    // updating Current Date if we are recording longer than a day
    // Capture Current Date
    nowYear = timeinfo.tm_year + 1900;
    nowMonth = timeinfo.tm_mon + 1;
    nowDay = timeinfo.tm_mday;

    // Refreshing all the variables for the next day
    if (todayDateCheck != nowDay)
    {
      // Refresh the Boolean flag that records the email sent in a day
      isTodayEmailSent = 0;
      // Refresh the Date
      dateString = dateFormat(nowYear, nowMonth, nowDay);
      // Refresh the total Body Counter
      totalBodyCtr = 0;
      prevTotalBodyCt = 0;
      tempHourlyBodyCtr = 0;
      // Refresh Message Number
      msgNo = 0;
      // Refresh the HTML message list
      htmlMsg =
          "<div>"
          "<style>"
          "table {"
          "font-family: arial, sans-serif;"
          "border-collapse: collapse;"
          "width: 100%;"
          "}"

          "td, th {"
          "border: 1px solid #dddddd;"
          "text-align: left;"
          "padding: 8px;"
          "}"

          "tr:nth-child(even) {"
          "background-color: #dddddd;"
          "}"
          "</style>"
          "<table style=\"width:100%\">"
          "<tr>"
          "<th>Date</th>"
          "<th>Start from</th>"
          "<th>to</th>"
          "<th>units</th>"
          "</tr>"
          "</table>"
          "</div>";
      // Refresh the html Message used for production fall
      String htmlMsgProdShort =
          "<div>"
          "<style>"
          "table {"
          "font-family: arial, sans-serif;"
          "border-collapse: collapse;"
          "width: 100%;"
          "}"

          "td, th {"
          "border: 1px solid #dddddd;"
          "text-align: left;"
          "padding: 8px;"
          "}"

          "tr:nth-child(even) {"
          "background-color: #dddddd;"
          "}"
          "</style>"
          "<p> [TODAY DATE]: Total bodies foamed upto now = 0 units</p>"
          "<table style=\"width:100%\">"
          "<tr>"
          "<th>Date</th>"
          "<th>Start from</th>"
          "<th>to</th>"
          "<th>units</th>"
          "</tr>"
          "</table>"
          "</div>";
    }

    // Serial Print only if a body is detected
    if (tempHourlyBodyCtr != hourlyBodyCtr)
    {
      Serial.printf("Hourly Body Count outside email: %d\n", hourlyBodyCtr);
      Serial.printf("Total Body Count outside email: %d\n", totalBodyCtr);
    }
    // Collect data for email - if: 1. An hour has passed; 2. It is 4.30pm
    if ((lastHrVar != nowHr) || ((nowHr == 16) && (nowMin == 30) && (nowSec == 0)))
    {
      Serial.printf("------------------------------ SENDING EMAIL NOW -----------------------------\n");
      lastHrVar = nowHr;
      bodyRecTimeStop = timeFormat(nowHr, nowMin, nowSec);
      // ------------------------ Invoking Email Reporting Function -------
      // Note: This does not always send the email - check the function for more details
      sendMsg(dateString, bodyRecTimeStart, bodyRecTimeStop, hourlyBodyCtr, totalBodyCtr, prevTotalBodyCt, nowHr, nowMin, nowSec);
      // ------------------------ End of Email Reporting Function ---------
      prevTotalBodyCt = totalBodyCtr;
      Serial.println("An hour has passed");
      Serial.printf("%02d:%02d:%02d\n", nowHr, nowMin, nowSec);
      Serial.printf("Hourly Body Count is %d\n", hourlyBodyCtr);
      Serial.printf("Total.... Body Count is %d\n", totalBodyCtr);
      hourlyBodyCtr = 0;
    }
    tempHourlyBodyCtr = hourlyBodyCtr;
  }
}

// ------------------------------------ Callback functions -----------------------
// sendMsg Function
// Inputs:
// 1. dateVal(String)       -   Today's date
// 2. timeStart(String)     -   The Time when the System ie the ESP32 had started/received power
// 3. timeStop(String)      -   The time when the email for the last hour is going to be sent
// 4. units(integer)        -   The total count of refrigerator bodies in an hour
// 5. totalUnits(integer)   -   The total count of Refrigerator Bodies in a day
// 6. pastHrUnits(integer)  -   The last value of totalUnits is stored here
// 7,8,9. currentHr, currentMin, currentSec(int) -    The most recent Hour, min and second in which the program execution enters this function

// Outputs: None - These are the "return" variables

void sendMsg(String dateVal, String timeStart, String timeStop, int units, int totalUnits, int pastHrUnits, int currentHr, int currentMin, int currentSec)
{
  /* Declare the message class which holds the whole report*/
  SMTP_Message message;

  /* Declare the specialMessage class which holds the report if production falls below 50*/
  SMTP_Message specialMessage;

  /* Set the message headers */
  message.sender.name = F("Foamed Body Counter");
  message.sender.email = AUTHOR_EMAIL;
  message.subject = F("Body Count at Body Foaming from SRP(ex-IAL)");
  message.addRecipient(RECIPIENT_NAMES[0], RECIPIENT_EMAILS[0]); // Abu Bakr Siddique
  message.addRecipient(RECIPIENT_NAMES[1], RECIPIENT_EMAILS[1]); // Helal Uddin
  message.addRecipient(RECIPIENT_NAMES[2], RECIPIENT_EMAILS[2]); // Hakan Altinisik
  message.addRecipient(RECIPIENT_NAMES[3], RECIPIENT_EMAILS[3]); // Tanvir Ahmed Fahim
  // Added these on 15th October
  message.addRecipient(RECIPIENT_NAMES[4], RECIPIENT_EMAILS[4]);   // Saygin Yurtseven
  message.addRecipient(RECIPIENT_NAMES[5], RECIPIENT_EMAILS[5]);   // Mohammed Ahsanul Kabir
  message.addRecipient(RECIPIENT_NAMES[6], RECIPIENT_EMAILS[6]);   // Moniruzzaman Mia
  message.addRecipient(RECIPIENT_NAMES[7], RECIPIENT_EMAILS[7]);   // Samiha Mustabin Jaigirdar
  message.addRecipient(RECIPIENT_NAMES[8], RECIPIENT_EMAILS[8]);   // Redwan Ali Sheikh
  message.addRecipient(RECIPIENT_NAMES[9], RECIPIENT_EMAILS[9]);   // Mumtarin Rahman
  message.addRecipient(RECIPIENT_NAMES[10], RECIPIENT_EMAILS[10]); // Shahinur Islam
  message.addRecipient(RECIPIENT_NAMES[11], RECIPIENT_EMAILS[11]); // Abul Basar
  // Added these on 11th December
  message.addRecipient(RECIPIENT_NAMES[12], RECIPIENT_EMAILS[12]); // Maliha Tasnim
  message.addRecipient(RECIPIENT_NAMES[13], RECIPIENT_EMAILS[13]); // Meghla Roy

  // ------------------------------------- Modified Code -------------------------------------
  /*Modify HTML message only if time is between 7am - 4.30pm */
  if ((currentHr >= 7) && (currentHr <= 16 && (currentMin <= 30 && currentMin >= 0)))
  {
    htmlMsg.replace("</table>", String(""));
    htmlMsg.replace("</div>", String(""));
    htmlMsg +=
        "<tr>"
        "<td>[TABLE DATE]</td>"
        "<td>[START TIME]</td>"
        "<td>[END TIME]</td>"
        "<td>[UNITS]</td>"
        "</tr>"
        "</table>"
        "</div>";
    // ------------------------------------- Modified Code -------------------------------------
    // ------------------------------------- Added Code -------------------------------------
    // Replacing the contents inside the [] with actual values
    htmlMsg.replace("[TABLE DATE]", dateVal);
    htmlMsg.replace("[START TIME]", timeStart);
    htmlMsg.replace("[END TIME]", timeStop);
    htmlMsg.replace("[UNITS]", String(units));
    htmlMsg.replace("[TODAY DATE]", dateVal);
    htmlMsg.replace((String(pastHrUnits) + " units"), (String(totalUnits) + " units"));
  }

  // ------------------------------------- Added Code -------------------------------------
  message.html.content = htmlMsg.c_str();
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  // message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  /* Connect to the server */
  if (!smtp.connect(&config))
  {
    ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!smtp.isLoggedIn())
  {
    Serial.println("\nNot yet logged in.");
  }
  else
  {
    if (smtp.isAuthenticated())
      Serial.println("\nSuccessfully logged in.");
    else
      Serial.println("\nConnected with no Auth.");
  }

  /* Send Final Email only at 4.30pm and if it has not been sent already once today and then close session each time */
  if ((currentHr == 16 && currentMin == 30) && (isTodayEmailSent == 0))
  {
    if (!MailClient.sendMail(&smtp, &message))
    {
      Serial.println("Trying to send final email\n");
      ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
      isTodayEmailSent = 1; // Turn this flag to 1 after we have sent the email for the day
    }
  }
  // Now the part for if the production is below 50 units per hour
  /* Setting email headers if production falls short*/
  specialMessage.sender.name = F("Foamed Body Counter");
  specialMessage.sender.email = AUTHOR_EMAIL;
  specialMessage.subject = F("Production Alert - Reduced Output in Body Foaming from SRP(ex-IAL)");
  specialMessage.addRecipient(RECIPIENT_NAMES[0], RECIPIENT_EMAILS[0]); // Abu Bakr Siddique
  specialMessage.addRecipient(RECIPIENT_NAMES[2], RECIPIENT_EMAILS[2]); // Hakan Altinisik
  specialMessage.addRecipient(RECIPIENT_NAMES[1], RECIPIENT_EMAILS[1]); // Helal Uddin
  specialMessage.addRecipient(RECIPIENT_NAMES[3], RECIPIENT_EMAILS[3]); // Tanvir Ahmed Fahim
  // Added these on 15th October
  specialMessage.addRecipient(RECIPIENT_NAMES[5], RECIPIENT_EMAILS[5]);   // Mohammed Ahsanul Kabir
  specialMessage.addRecipient(RECIPIENT_NAMES[4], RECIPIENT_EMAILS[4]);   // Saygin Yurtseven
  specialMessage.addRecipient(RECIPIENT_NAMES[6], RECIPIENT_EMAILS[6]);   // Moniruzzaman Mia
  specialMessage.addRecipient(RECIPIENT_NAMES[7], RECIPIENT_EMAILS[7]);   // Samiha Mustabin Jaigirdar
  specialMessage.addRecipient(RECIPIENT_NAMES[8], RECIPIENT_EMAILS[8]);   // Redwan Ali Sheikh
  specialMessage.addRecipient(RECIPIENT_NAMES[9], RECIPIENT_EMAILS[9]);   // Mumtarin Rahman
  specialMessage.addRecipient(RECIPIENT_NAMES[10], RECIPIENT_EMAILS[10]); // Shahinur Islam
  specialMessage.addRecipient(RECIPIENT_NAMES[11], RECIPIENT_EMAILS[11]); // Abul Basar
  // Added these on 11th December
  specialMessage.addRecipient(RECIPIENT_NAMES[12], RECIPIENT_EMAILS[12]); // Maliha Tasnim
  specialMessage.addRecipient(RECIPIENT_NAMES[13], RECIPIENT_EMAILS[13]); // Meghla Roy
  if ((units < 50) && ((currentHr >= 7) && (currentHr <= 16 && (currentMin <= 30 && currentMin >= 0))))
  {

    htmlMsgProdShort =
        "<div>"
        "<style>"
        "table {"
        "font-family: arial, sans-serif;"
        "border-collapse: collapse;"
        "width: 100%;"
        "}"

        "td, th {"
        "border: 1px solid #dddddd;"
        "text-align: left;"
        "padding: 8px;"
        "}"

        "tr:nth-child(even) {"
        " background-color: #dddddd;"
        "}"
        "</style>"
        "<p> The Production in the last hour is below 50 units</p>"
        "<table style=\"width:100%\">"
        "<tr>"
        "<th>Date</th>"
        "<th>Start from</th>"
        "<th>to</th>"
        "<th>units</th>"
        "</tr>"
        "<tr>"
        "<td>[TABLE DATE]</td>"
        "<td>[START TIME]</td>"
        "<td>[END TIME]</td>"
        "<td>[UNITS]</td>"
        "</tr>"
        "</table>"
        "</div>";
    htmlMsgProdShort.replace("[TABLE DATE]", dateVal);
    htmlMsgProdShort.replace("[START TIME]", timeStart);
    htmlMsgProdShort.replace("[END TIME]", timeStop);
    htmlMsgProdShort.replace("[UNITS]", String(units));
    specialMessage.html.content = htmlMsgProdShort.c_str();
    specialMessage.text.charSet = "us-ascii";
    specialMessage.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
    // specialMessage.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
    specialMessage.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;
    // Sending Email - some of the prior setup was done outside this conditional, so they haven't been included in here.
    Serial.println("Captured the actual problem");
    if (!MailClient.sendMail(&smtp, &specialMessage))
    {
      Serial.println("Trying to send unusual email\n");
      ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    }
  }
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {

    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (msgNo = 0; msgNo < smtp.sendingResult.size(); msgNo++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(msgNo);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)

      ESP_MAIL_PRINTF("Message No: %d\n", msgNo + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
    // --------------------------- No Changes to the above function smtpCallback ----------------------
  }
}

String dateFormat(int yearVal, int monthVal, int dayVal)
{
  String dateVal;
  if ((dayVal < 10) || (monthVal < 10))
  {
    if (dayVal < 10)
    {
      dateVal = String("0") + String(dayVal);
    }
    else
    {
      dateVal = String(dayVal);
    }

    if (monthVal < 10)
    {
      dateVal += "." + String("0") + String(monthVal) + "." + String(yearVal);
    }
    else
    {
      dateVal += "." + String(monthVal) + "." + String(yearVal);
    }
  }
  else
  {
    dateVal = String(dayVal) + "." + String(monthVal) + "." + String(yearVal);
  }
  return dateVal;
}

String timeFormat(int hrVal, int minVal, int secVal)
{
  String timeVal;
  if (hrVal < 10 || minVal < 10 || secVal < 10)
  {
    if (hrVal < 10)
    {
      timeVal = String("0") + String(hrVal);
    }
    else
    {
      timeVal = String(hrVal);
    }

    if (minVal < 10)
    {
      timeVal += ":" + String("0") + String(minVal);
    }
    else
    {
      timeVal += ":" + String(minVal);
    }

    if (secVal < 10)
    {
      timeVal += ":" + String("0") + String(secVal);
    }
    else
    {
      timeVal += ":" + String(secVal);
    }
  }
  else
  {
    timeVal = String(hrVal) + ":" + String(minVal) + ":" + String(secVal);
  }
  return timeVal;
}