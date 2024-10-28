#include <ArduinoBLE.h>

const int GSR_PIN = A0;                   // GSR 센서 핀
const int SAMPLE_INTERVAL_HR = 20;         // 심박수 샘플링 간격 (50Hz)
const int SAMPLE_INTERVAL_GSR = 5;         // GSR 샘플링 간격 (200Hz)
int heartRateSamples[3000];                // 1분 동안의 심박수 샘플 배열 (50Hz x 60초)
int gsrSamples[12000];                     // 1분 동안의 GSR 샘플 배열 (200Hz x 60초)
int hrIndex = 0;
int gsrIndex = 0;

// 초기 평균 및 표준편차 값 (초기값 가정)
float hrMean = 75.0;
float hrStdDev = 5.0;
float gsrMean = 500.0;
float gsrStdDev = 20.0;

int tensionCounter = 0;                    // 긴장 상태 카운터
int joyCounter = 0;                        // 기쁨 상태 카운터

BLEService stressService("180D");          // BLE 서비스
BLEIntCharacteristic hrCharacteristic("2A37", BLERead | BLENotify);  // 심박수 특성
BLEIntCharacteristic gsrCharacteristic("2A38", BLERead | BLENotify); // GSR 특성
BLEStringCharacteristic stateCharacteristic("2A39", BLERead | BLENotify, 20); // 현재 상태
BLEStringCharacteristic alertCharacteristic("2A3B", BLERead | BLENotify, 20); // 경고 알림


String determineState() {
    bool hrHigh = hrMean >= hrMean + hrStdDev;
    bool gsrHigh = gsrMean >= gsrMean + gsrStdDev;

    if (hrHigh && gsrHigh) return "Tension";
    else if (hrHigh && !gsrHigh) return "Joy";
    else return "Neutral";
}

String determineImmediateState() {
    bool hrHigh = hrMean >= hrMean + 2 * hrStdDev;
    bool gsrHigh = gsrMean >= gsrMean + 2 * gsrStdDev;

    if (hrHigh && gsrHigh) return "Tension";
    else if (hrHigh && !gsrHigh) return "Joy";
    else return "Neutral";
}

float calculateMean(int* samples, int size) {
    long sum = 0;
    for (int i = 0; i < size; i++) sum += samples[i];
    return (float)sum / size;
}

float calculateStdDev(int* samples, int size, float mean) {
    float variance = 0;
    for (int i = 0; i < size; i++) variance += pow(samples[i] - mean, 2);
    return sqrt(variance / size);
}

void calculateStatistics() {
    hrMean = calculateMean(heartRateSamples, 3000);
    hrStdDev = calculateStdDev(heartRateSamples, 3000, hrMean);
    gsrMean = calculateMean(gsrSamples, 12000);
    gsrStdDev = calculateStdDev(gsrSamples, 12000, gsrMean);
}

void setup() {
    Serial.begin(115200);

    if (!BLE.begin()) {
        Serial.println("BLE 시작 실패");
        while (1);
    }
    BLE.setLocalName("Nano33BLE_Sensor");
    BLE.setAdvertisedService(stressService);
    stressService.addCharacteristic(hrCharacteristic);
    stressService.addCharacteristic(gsrCharacteristic);
    stressService.addCharacteristic(stateCharacteristic);
    stressService.addCharacteristic(alertCharacteristic);
    BLE.addService(stressService);
    BLE.advertise();
    Serial.println("BLE 장치가 시작되었습니다.");
}

void loop() {
    unsigned long currentMillis = millis();

    // 심박수 데이터 수집
    if (currentMillis % SAMPLE_INTERVAL_HR == 0) {
        int hrValue = analogRead(A1);
        heartRateSamples[hrIndex++] = hrValue;
        if (hrIndex >= 3000) hrIndex = 0;
    }

    // GSR 데이터 수집
    if (currentMillis % SAMPLE_INTERVAL_GSR == 0) {
        int gsrValue = analogRead(GSR_PIN);
        gsrSamples[gsrIndex++] = gsrValue;
        if (gsrIndex >= 12000) gsrIndex = 0;
    }

    // 매 1분마다 평균 및 표준편차 계산
    if (currentMillis % 60000 == 0) {
        calculateStatistics();

        // 현재 상태 확인 및 BLE 알림
        String currentState = determineState();
        hrCharacteristic.writeValue((int)hrMean);
        gsrCharacteristic.writeValue((int)gsrMean);
        stateCharacteristic.writeValue(currentState);

        // 15분 유효 구간 확인
        if (currentState == "Tension") {
            joyCounter = 0;
            tensionCounter++;
        } else if (currentState == "Joy") {
            tensionCounter = 0;
            joyCounter++;
        } else {
            tensionCounter = 0;
            joyCounter = 0;
        }

        // 유효 구간 BLE 알림
        if (tensionCounter >= 15) {
            alertCharacteristic.writeValue("Valid Tension Period Detected");
        } else if (joyCounter >= 15) {
            alertCharacteristic.writeValue("Valid Joy Period Detected");
        }
    }

    // 즉시 알림: 표준편차 x2 조건 확인
    if (abs(hrMean - hrMean) >= 2 * hrStdDev || abs(gsrMean - gsrMean) >= 2 * gsrStdDev) {
        String immediateState = determineImmediateState();
        alertCharacteristic.writeValue("Alert: " + immediateState);
    }

    BLE.poll();
}

