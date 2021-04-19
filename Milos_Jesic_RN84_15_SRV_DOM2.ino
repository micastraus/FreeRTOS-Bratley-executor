#include <Arduino_FreeRTOS.h>

#define a_char_dec 97
#define uzvicnik_char_dec 33
#define zero_char_dec 48

#define SLOVA_INTERVAL 30
#define BROJEVI_INTERVAL  60
#define ZNAKOVI_INTERVAL  90

void Task1( void *pvParameters    );
void Task2( void *pvParameters );

TaskHandle_t xTask1Handle;
TaskHandle_t xTask2Handle;

volatile char s[100];     // niz sa parametrima za svaki posao
volatile int serSize = 0; // duzina ulaza (Serial)
volatile int brojacPoslova = 0;

typedef struct job {
  int start;
  int duration;
  int deadline;
  char type;
} job;

typedef struct realJob {
  int start;
  int deadline;
  char type;
} realJob;

volatile job jobs[5];
volatile realJob realJobs[5];
volatile boolean visited[5];
volatile boolean losRaspored = false;
volatile boolean jobDone = false;
volatile long jobsStartedTick;
volatile int currExecuting;
volatile long jobEnd;

void setup() {
  Serial.begin(9600);
  xTaskCreate(Task1, (const portCHAR *)"Task1", 200, NULL, 2, &xTask1Handle );
  xTaskCreate(Task2, (const portCHAR *)"Task2", 200, NULL, 1, &xTask2Handle );
}

void loop() {
  Serial.println(F("IDLE"));
}

// [0,156,230,S];[0,40,200,B];[200,402,602,#];[602,380,1000,S];
// [100,156,330,S];[400,340,800,B];[900,30,930,#];[0,56,98,S];[0,40,45,B];
// [0,56,230,S];[0,40,200,B];[0,30,200,#];[0,56,98,S];[0,40,45,B];
// [4,2,7,S];[1,1,5,B];[1,2,6,#];[0,2,4,S];

void Task1(void *pvParameters) {
  while (true) {

    serSize = 0;

    brojacPoslova = 0;

    losRaspored = false;

    while (! Serial.available()) { }

    while (true) {
      if ( Serial.available()  ) {
        s[serSize] = Serial.read();
        if (s[serSize] == '\n') break;
        serSize++;
      }
    }

    int i = 0;
    for (i = 0; i < serSize; i++) {
      Serial.print(s[i]);
    }
    Serial.println();

    // Parsiranje Seriala i provera tacnosti parametara

    i = 0;
    while (i < serSize) {
      if (brojacPoslova >= 5) {
        Serial.println(F("Broj poslova ne sme biti veci od 5!"));
        losRaspored = true;
        break;
      }

      if (s[i] != '[') {
        Serial.println(F("Greska kod '[' !"));
        losRaspored = true;
        break;
      }
      i++;

      int startTick = 0, durationTick = 0, deadlineTick = 0;
      char jobChar = '0';

      for (int j = 0; j < 3; j++) {
        int tmpTick = 0;

        if (s[i] < '0' || s[i] > '9') {
          Serial.println(F("Greska kod vrednosti tick-a!"));
          losRaspored = true;
          break;
        }

        while (s[i] >= '0' && s[i] <= '9' && tmpTick <= 1000) {
          tmpTick = tmpTick * 10 + (s[i] - '0');
          i++;
        }
        if (s[i] != ',' || tmpTick > 1000) {
          Serial.println(F("Greska kod zapete ili vrednosti tick-ova!"));
          losRaspored = true;
          break;
        }
        i++;
        if (j == 0) startTick = tmpTick;
        else if (j == 1) durationTick = tmpTick;
        else if (j == 2) deadlineTick = tmpTick;
      }

      if (losRaspored) break;

      if (s[i] != 'S' && s[i] != 'B' && s[i] != '#') {
        Serial.println(F("Greska kod char (S,B,#)!"));
        losRaspored = true;
        break;
      }
      jobChar = s[i];
      i++;

      if (s[i] != ']') {
        Serial.println(F("Greska kod ']' !"));
        losRaspored = true;
        break;
      }
      i++;

      if (s[i] != ';') {
        Serial.println(F("Greska kod ';' !"));
        losRaspored = true;
        break;
      }
      i++;

      jobs[brojacPoslova].start = startTick;
      jobs[brojacPoslova].duration = durationTick;
      jobs[brojacPoslova].deadline = deadlineTick;
      jobs[brojacPoslova].type = jobChar;
      brojacPoslova++;
    }

    if (losRaspored) continue;

    // Zavrseno parsiranje Seriala

    for (int j = 0; j < brojacPoslova; j++) {
      if (jobs[j].start + jobs[j].duration > jobs[j].deadline) {
        Serial.println(F("Poslovi nisu rasporedivi (Kod nekog taska je trajanje > rok - start)!"));
        losRaspored = true;
        break;
      }
    }

    if (losRaspored) continue;

    for (int j = 0; j < brojacPoslova; j++) {
      visited[j] = true;
      realJobs[j].start = jobs[j].start;
      realJobs[j].deadline = jobs[j].start + jobs[j].duration;
      realJobs[j].type = jobs[j].type;
      if (bratleyRasporediv(realJobs[j].deadline , 1)) {  // saljemom vrednost 1 za scheduledJobs, jer je svaki task validan
        Serial.println(F("Poslovi su rasporedivi."));

        // sortiramo poslove po pocetku

        for (int a = 0; a < brojacPoslova - 1; a++) {
          for (int b = a + 1; b < brojacPoslova; b++) {
            if (realJobs[a].start > realJobs[b].start) {
              int tmpStart = realJobs[a].start;
              int tmpDeadline = realJobs[a].deadline;
              int tmpType = realJobs[a].type;
              realJobs[a].start = realJobs[b].start;
              realJobs[a].deadline = realJobs[b].deadline;
              realJobs[a].type = realJobs[b].type;
              realJobs[b].start = tmpStart;
              realJobs[b].deadline = tmpDeadline;
              realJobs[b].type = tmpType;
            }
          }
        }

        for (int a = 0; a < brojacPoslova; a++) {
          Serial.print(realJobs[a].start);
          Serial.print(";");
          Serial.print(realJobs[a].deadline);
          Serial.print(";");
          Serial.print(realJobs[a].type);
          Serial.println(";");
        }


        long jobsEndTick[brojacPoslova];
        jobsStartedTick = xTaskGetTickCount();
        currExecuting = 0;
        for (currExecuting = 0; currExecuting < brojacPoslova; currExecuting++) {

          jobDone = false;

          while (jobsStartedTick + realJobs[currExecuting].start > xTaskGetTickCount()) {
            // Cekamo da dodje vreme za start sledeceg posla
          }

          vTaskPrioritySet(xTask2Handle, 3);

          while (jobDone == false) {
            // Cekamo da se trenutni posao zavrsi
          }

          jobsEndTick[currExecuting] = jobEnd;
        }

        Serial.print(F("\nPoslovi poceli posle tika: "));
        Serial.println(jobsStartedTick);
        for (int k = 0; k < brojacPoslova; k++) {
          Serial.print(k + 1);
          Serial.print(F(". se zavrsio posle tika: "));
          Serial.println(jobsEndTick[k]);
        }

        break;  // PAZI SE OVDE !!! ( OK je, nakon vTaskPrioritySet(),
        // nastavlja se od prve sledece linije unutar nase rutine)
      }
      visited[j] = false;

      if (j == brojacPoslova - 1) { // Znaci da nije rasporediv skup poslova
        Serial.println(F("Poslovi nisu rasporedivi!"));
      }
    }
    for (int j = 0; j < brojacPoslova; j++) {
      realJobs[j].start = 0;
      realJobs[j].deadline = 0;
      realJobs[j].type = 0;
      visited[j] = false;
    }
  }
}

boolean bratleyRasporediv(int prevJobEnd, int scheduledJobs) {
  if (scheduledJobs == brojacPoslova) return true;

  int i = 0;
  for (i = 0; i < brojacPoslova; i++) {
    if (visited[i]) continue;

    if (jobs[i].start <= prevJobEnd && prevJobEnd + jobs[i].duration <= jobs[i].deadline
        || jobs[i].start > prevJobEnd) {
      int tmpJobRealStart = (jobs[i].start > prevJobEnd) ? jobs[i].start : prevJobEnd;
      realJobs[i].start = tmpJobRealStart;
      realJobs[i].deadline = tmpJobRealStart + jobs[i].duration;
      realJobs[i].type = jobs[i].type; // zbog nedostatka ove linije, bio je bag
      visited[i] = true;
      boolean scheduled = bratleyRasporediv(realJobs[i].deadline, scheduledJobs + 1);
      if (scheduled) return true;
      visited[i] = false;
    }
    else return false;
  }

  visited[i - 1] = false;
  return false;
}

void Task2( void *pvParameters ) {
  while (true) {
    char jobType = realJobs[currExecuting].type;
    int print_interval = 0;
    char txtPtr = 0;

    if (jobType == 'S') {
      print_interval = SLOVA_INTERVAL;
      txtPtr = a_char_dec;
    }
    else if (jobType == 'B') {
      print_interval = BROJEVI_INTERVAL;
      txtPtr = zero_char_dec;
    }
    else if (jobType == '#') {
      print_interval = ZNAKOVI_INTERVAL;
      txtPtr = uzvicnik_char_dec;
    }
    else {
      Serial.println(F("Posao nema validan tip (S,B,#) !"));
      continue;
    }

    while (true) {
      Serial.print(txtPtr);
      long currTick = xTaskGetTickCount();
      if (currTick + (print_interval / portTICK_PERIOD_MS) > jobsStartedTick + realJobs[currExecuting].deadline) {
        jobDone = true;
        jobEnd = currTick;
        vTaskPrioritySet(NULL, 1);
        break;
      }

      vTaskDelay(print_interval / portTICK_PERIOD_MS);

      if (txtPtr == 'z') txtPtr = a_char_dec;
      else if (txtPtr == '9') txtPtr = zero_char_dec;
      else if (txtPtr == '%') txtPtr = uzvicnik_char_dec;
      else txtPtr++;
    }
    Serial.println();
  }
}
