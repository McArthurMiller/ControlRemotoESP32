/*
 *(V1) Teclado 4x4 + LCD 16X2 + Protcolo serial (STX/ETX)
   TECLADO: Lee el teclado por multiplexado. Activa una fila a la vez y revisa las 4 columnas buscando cual cayo. Devuelve el caracter correspondiente o '\0' si no hay nada presionado.
   Comunicación serial Comandos: Enviar: tecla'A', comando(byte) 0x2A; Borrar numero: tecla 'B', comando(byte) 0x55; Borrar todo: tecla 'C' confirmacion con '1' y cancelar '2', limpiar LCD: tecla '*';
   test: tecla '#' comando(byte)0x23 salir de test 'D'. PANTALLA: limpiarBuffer resetea el buffer a 3 espacios vacios.
   limpiarPantallaLCD limpia la segunda fila del LCD y reinicia el buffer y la memoria del ultimo numero (fuerza el estado vacio tras limpiar). SETUP():Inicializa Serial el LCD configura las filas del teclado como Salida y
   las columnas como Entrada con Pull-Up interno. LOOP(): Cada 'debounceDelay'`milisegundos escanea el teclado y solo si la tecla detectada es distinta a la anterior, la procesa con procesarTecla()
 *(V2) SENSOR: Se utilizo sensor DS18B20 (OneWire + DallasTemperature, solo temperatura) - PIN de datos 17 - Lectura: sensores.requestTemperatures() + getTempCByIndex(0) PANTALLA: Esta versión limpia y reescribe temperatura:
   columna 0(7 espacios). TECLADO: Maneja la lista de numeros enviados: agregar al final (si hay espacio), buscar y quitar un número especifico, o vaciar toda la lista reiniciando el contador.
   MANEJO DE ERRORES DEL SENSOR: V2: (tempC == DEVICE_DISCONNECTED_C). SETUP / SERIAL: Esta versión reporta por Serial cuantos sensores DS18B20 se encontraron (getDeviceCount()).
   LOOP(): este bloque revisa en cada vuelta del loop() si ya pasó suficiente tiempo (10 segundos) desde la última lectura del sensor, actualiza la pantalla y reinicia el contador sin usar delay().
 *(V3) TECLADO + SENSOR DHT22 (TEMPERATURA + HUMEDAD): SENSOR: Se reemplazo el DS18B20 por un DHT22 (libreria DHT, temperatura y humedad).
   Lectura: de sensores.requestTemperatures() + getTempCByIndex(0) a dht.readHumidity() + dht.readTemperature(). REFRESCO DE PANTALLA: Esta versión limpia y reescribe dos zonas independientes:
   temperatura en columna 0 (7 espacios) y humedad en columna 9 (8 espacios). MANEJO DE ERRORES DEL SENSOR: V3: isnan(h) || isnan(t) (propio de la libreria DHT), cubriendo la lectura de ambos valores.
   SETUP / SERIAL: La V3 ya no tiene sentido de "conteo de dispositivos" con DHT.
  *(V4) TECLADO + SENSOR DHT22 (TEMPERATURA + HUMEDAD): Se utiliza un solo sensor DHT22 (libreria DHT, temperatura y humedad) en el pin de datos 17.
   REFRESCO DE PANTALLA: La versión 4 imprime temperatura y humedad en la fila 0 del LCD; la fila 1 queda reservada para los mensajes del teclado. SETUP / SERIAL: Las versiones 3 y 4 reportan por serial.
*/


//Librerias y Configuración del sensor
#include <LiquidCrystal.h>
#include <DHT.h>

#define DHTPIN 17
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

unsigned long ultimaLecturaTemp = 0;
const unsigned long intervaloLectura = 10000;

//Pantalla
LiquidCrystal lcd (4,5,18,19,21,22);

const int filas[4] = {26,25,33,32};
const int columnas[4] = { 13,12,14,27};

//Mapa del teclado
char teclas[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

char teclaAnterior = '\0';
unsigned long ultimaLectura = 0;
const unsigned long debounceDelay = 50;

//Lista de numeros activos
const unsigned long Max_numeros = 10;
int numerosEnviados[Max_numeros];
int totalEnviados = 0;

//Buffer del teclado
char buffer[4] = {' ',' ',' ','\0'};  //maximo 3 posiciones
bool bufferVacio = true;

//Bandera de estado del sistema
bool esperandoConfirmacion = false;

//Variable del modo prueba
bool testActivo = false;

//Memoria del ultimo número
int ultimoNumero = -1;
bool hayUltimoNumero = false;    

void setup() {
  Serial.begin(9600);
  dht.begin();
  lcd.begin(16,2);
  lcd.setCursor(0,0);
  lcd.print("                ");

  //Filas del teclado como SALIDA, arranca en HIGH(Inactiva)
  for (int i = 0; i < 4; i++) {
    pinMode(filas[i], OUTPUT);
    digitalWrite(filas[i], HIGH);
  }

  //Columnas del teclado como ENTRADA con resistencia PULLUP interna
  for (int i = 0; i < 4; i++) {
    pinMode(columnas[i], INPUT_PULLUP);
  }

  Serial.println("Sensor");
   
}

//Escanear Teclado
//Lectura por Multiplexado  
char escanearTeclado() {
  for (int f = 0; f < 4; f++) {
    digitalWrite(filas[f], LOW);
    for (int c = 0; c < 4; c++) {
      if (digitalRead(columnas[c]) == LOW) {
        digitalWrite(filas[f], HIGH);  //Desactiva la fila antes de Salir
        return teclas[f][c];      
    }
    }
    digitalWrite(filas[f], HIGH);  //Desactiva la fila si no hubo match
  }
  return '\0';  //ningún botón presionado
}


void agregarNumeroEnviado(int numero) {
  if (totalEnviados < Max_numeros) {
    numerosEnviados[totalEnviados] = numero;
    totalEnviados++;
  }
}

//Busca y quita un número especifico de la lista
bool quitarNumeroEnviado(int numero) {  
  int posicion = -1;
  for (int i = 0; i < totalEnviados; i++) {
    if (numerosEnviados[i] == numero) {
      posicion = i;
      break;
    }
  }
  if (posicion == -1) {
    return false;  //No estaba en la lista
  }
  for (int i = posicion; i < totalEnviados - 1; i++) {
    numerosEnviados[i] = numerosEnviados[i + 1];
  }
  totalEnviados--;
  return true;
}

//Vacia toda la lista (solo reinicia el contador)
void limpiarTodosLosEnviados () {
  totalEnviados = 0;
}

//Buffer deslizante (FIFO 3 posiciones)
void agregarABuffer(char nuevaTecla) {  
  buffer[0] = buffer[1];
  buffer[1] = buffer[2];
  buffer[2] = nuevaTecla;
  bufferVacio = false;
  mostrarBufferLCD();
}

void enviarTrama(byte comando, int numero, bool incluirSegmento) {
  Serial.write((byte) 0x02);  //STX

  if (incluirSegmento) {
    char numeroTexto[4];
    sprintf(numeroTexto, "%03d", numero);  

    for (int i = 0; i < 3; i++) {
      Serial.write((byte) numeroTexto[i]);
    }
    Serial.write(comando);
  } else {
    Serial.write(comando);
  }
  Serial.write((byte) 0x03);  //ETX
}

void enviarTramaVacia(byte comando) {
  Serial.write((byte) 0x02);  //STX
  Serial.write((byte) 0x20);
  Serial.write((byte) 0x20);
  Serial.write((byte) 0x20);
  Serial.write(comando);
  Serial.write((byte) 0x03);  //ETX
}

void enviarTramaTest() {
  Serial.write((byte) 0x02);
  Serial.write((byte) 0x20);
  Serial.write((byte) 0x20);
  Serial.write((byte) 0x20);
  Serial.write((byte) 0x23);
  Serial.write((byte) 0x03);
}

void mostrarTestSerial() {
  enviarTramaTest();
}

void mostrarEnviadoLCD(int numero) {
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  String mensaje = String(numero) + " Enviado...";
  lcd.print(mensaje);
}

void mostrarBorradoLCD(char* numero) {  
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  String mensaje = String(numero) + " Borrado...";
  lcd.print(mensaje);
}

void mostrarBufferLCD() {
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print(buffer);
}

void mostrarPromptBorrar() {
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print("Borrar 1.Si 2.No");
}

void limpiarBuffer() {
  buffer[0] = ' ';
  buffer[1] = ' ';
  buffer[2] = ' ';
  bufferVacio = true;
}

void limpiarPantallaLCD() {
  lcd.setCursor(0,1);
  lcd.print("                ");

  limpiarBuffer();
  hayUltimoNumero = false;  
}

void mostrarTemperaturaSiInactivo() {
  if (!bufferVacio || esperandoConfirmacion) {
    return;  
  }

  float h = dht.readHumidity();  //humedad del sensor  (pin17)
  float t = dht.readTemperature();  //temperatura del sensor  (pin17)

  if (isnan(h) || isnan(t)) {
    Serial.print("Error al leer el sensor");
    return;
  }

  lcd.setCursor(0,0);
  lcd.print("       ");
  lcd.setCursor(0,0);
  lcd.print(String(t, 1));
  lcd.write((byte) 223);  
  lcd.print("C");

  lcd.setCursor(9,0);
  lcd.print("        ");
  lcd.setCursor(9,0);
  lcd.print(String(h, 1));
  lcd.print("%");
 
}

void procesarTecla(char tecla) {
  if (tecla == 'A') {
    int numActual;

    if (!bufferVacio) {
      numActual = atoi(buffer);  
      agregarNumeroEnviado(numActual);
      ultimoNumero = numActual;
      hayUltimoNumero = true;
      limpiarBuffer();

      mostrarEnviadoLCD(numActual);
      enviarTrama(0x2A, numActual, true);
    }
    else if (hayUltimoNumero) {
      numActual = ultimoNumero;

      mostrarEnviadoLCD(numActual);
      enviarTrama(0x2A, numActual, true);
    }
    else {
      lcd.setCursor(0,1);
      lcd.print("                ");
      lcd.setCursor(0,1);
      lcd.print(" ");
      enviarTramaVacia(0x2A);
    }
    return;
  }
  else if (tecla == 'B') {
    int numActual;

    if(!bufferVacio) {
      numActual = atoi(buffer);
      ultimoNumero = numActual;
      hayUltimoNumero = true;

      quitarNumeroEnviado(numActual);
      enviarTrama(0x55, numActual, true);

      char numeroTexto[6];
      itoa(numActual, numeroTexto, 10);
      mostrarBorradoLCD(numeroTexto);

      limpiarBuffer();
    }
    else if (hayUltimoNumero) {
      numActual = ultimoNumero;

      quitarNumeroEnviado(numActual);
      enviarTrama(0x55, numActual, true);

      char numeroTexto[6];
      itoa(numActual, numeroTexto, 10);
      mostrarBorradoLCD(numeroTexto);
    }
    else {
      lcd.setCursor(0,1);
      lcd.print("                ");
      lcd.setCursor(0,1);
      lcd.print(" ");
      enviarTramaVacia(0x55);
    }
  }
  else if (tecla == 'C') {
    if(!esperandoConfirmacion) {
      mostrarPromptBorrar();
      esperandoConfirmacion = true;
    }
  }
  else if(tecla == 'D') {
    if (testActivo) {
      testActivo = false;
    }
  }
  else if (tecla == '*') {
    limpiarPantallaLCD();
  }
  else if (tecla == '#') {
    testActivo = true;
    mostrarTestSerial();
  }
  else {
    if (esperandoConfirmacion) {
      if (tecla == '1') {
        limpiarTodosLosEnviados();
        lcd.setCursor(0,1);
        lcd.print("                ");
        lcd.setCursor(0,1);
        lcd.print("                ");
        enviarTramaVacia(0x43);
        esperandoConfirmacion = false;

        limpiarBuffer();
        hayUltimoNumero = false;
      }
      else if (tecla == '2') {
        lcd.setCursor(0,1);
        lcd.print("                ");
        lcd.setCursor(0,1);
        lcd.print("                ");
        esperandoConfirmacion = false;
      }
    }
    else if (tecla >= '0' && tecla <= '9') {
      agregarABuffer(tecla);
    }
  }
}

//Loop de teclado
void loop() {
  if (millis() - ultimaLectura > debounceDelay) {
    char teclaActual = escanearTeclado();

    if (teclaActual != '\0' && teclaActual != teclaAnterior) {
      procesarTecla(teclaActual);  
    }
    teclaAnterior = teclaActual;
    ultimaLectura = millis();
   
  }
//Loop de sensor de temperatura
  if (millis() - ultimaLecturaTemp >= intervaloLectura) {
    mostrarTemperaturaSiInactivo();
    ultimaLecturaTemp = millis();
  }
}
