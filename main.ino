#include <DHT.h>

// DHT Configuration
#define DHTPIN 2
#define DHTTYPE DHT11

// Relay Pin
#define RELAY_PIN 7

// Temperature Thresholds
#define TEMP_ON 30
#define TEMP_OFF 28

DHT dht(DHTPIN, DHTTYPE);

bool fanState = false;

void setup()
{
    Serial.begin(9600);

    pinMode(RELAY_PIN, OUTPUT);

    digitalWrite(RELAY_PIN, LOW);

    dht.begin();

    Serial.println("Smart Fan Controller Started");
}

void loop()
{
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity))
    {
        Serial.println("DHT11 Reading Failed!");
        delay(2000);
        return;
    }

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C  ");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    // Fan ON condition
    if (temperature >= TEMP_ON && !fanState)
    {
        digitalWrite(RELAY_PIN, HIGH);
        fanState = true;

        Serial.println("Fan ON");
    }

    // Fan OFF condition
    if (temperature <= TEMP_OFF && fanState)
    {
        digitalWrite(RELAY_PIN, LOW);
        fanState = false;

        Serial.println("Fan OFF");
    }

    delay(2000);
}
