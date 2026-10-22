//Arturo Jiménez Hernández 27/08/26

/* (V01) 27/08/26. Se crean variables globales y primer función enviarByte(); para mandar los bits uno por uno.
 * (V02) 31/08/26. se crea case y se corrige el loop() para poder encender los primeros 4 digitos de la tarjeta TARIF
 * (V03) 01/09/26. Función mostrarByte(); cuya función es pasar cada dato (byte) a la vez.
 * (V04) 01/09/26. Se agrega PWM para controlar el brillo
 * (V05) 02/09/26. Se agrega la funcion probarDigito(); cuya función es realizar un test mostrando el mismo número 4 veces durante 10 seg.
 * (V06) 02/09/26. Integración del teclado + tarjeta. Declaración de variables y se comienza a construir la logica y el codigo dentro del loop();
 * (V07) 03/09/26. Se realiza una rutina de recepción donde se cubre la trama del teclado: STX, 3 digitos, el comando y el ETX con validación y reinicio.
 * (V08) 04/09/26. Prueba de integración entre teclado 4x4 y tarjeta TARIF. La tarjeta responde y cambia conforme recibe la orden del teclado.
 */

#define CANAL 0
#define FRECUENCIA 5000
#define RESOLUCION 8


//Integración
int posicion = 0;
char buffer[4] = {' ', ' ', ' ', '\0'};
byte comandoRecibido;

//PIN conectado a ST_CP (registro de almacenamiento) del 74HC245
int latPin = 2;
//PIN conectado a SH_CP (registro de desplazamiento)
int clockPin = 26;
// PIN conectado a DS
int dataPin = 15;
int OEPin = 22;

int valorDeBrillo = 254;

void setup() {
  Serial.begin(9600);
 
  ledcSetup(CANAL, FRECUENCIA, RESOLUCION);
  ledcAttachPin(OEPin, CANAL);
  ledcWrite(CANAL, valorDeBrillo);
  // Se configuran los pines como salidas porque se direccionan en el bucle principal
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latPin, OUTPUT);
  pinMode(OEPin, OUTPUT);

  //Se mantiene en HIGH para manterla deshabiliatada
  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);
  digitalWrite(latPin, LOW);

  Serial.println("= Inicio =");

  mostrarByte(1234);
}


void enviarByte(int dato) {
  byte datoSalida;
 
  switch (dato) {
    case 0:
      datoSalida = 0x3F;
    break;

    case 1:
      datoSalida = 0x06;  
    break;

    case 2:
      datoSalida = 0x5B;
    break;

    case 3:
      datoSalida = 0x4F;
    break;

    case 4:
      datoSalida = 0x66;
    break;

    case 5:
      datoSalida = 0x6D;
    break;

    case 6:
      datoSalida = 0xFD;
    break;

    case 7:
      datoSalida = 0x07;
    break;

    case 8:
      datoSalida = 0x7F;
    break;

    case 9:
      datoSalida = 0x6F;
    break;
   
    default:
      Serial.println("Err");
      break;
    }
       
  shiftOut(dataPin, clockPin, MSBFIRST, datoSalida);  
 
}

void mostrarByte(int dato) {
  while (dato > 0) {
    int digito = dato % 10;
    enviarByte(digito);
    dato = dato / 10;
   
  }
 
  digitalWrite(latPin, HIGH);
  digitalWrite(latPin, LOW);
}

void probarDigito(int d) {
  for (int i = 0; i < 4; i++) {
    enviarByte(d);
  }

   digitalWrite(latPin, HIGH);
   digitalWrite(latPin, LOW);
   
}

void recepcionRutina() {
  if (Serial.available() > 0) {
    char byteRecibido = Serial.read();
   
    //Serial.print("Recibido: 0x");
    //Serial.println(byteRecibido, HEX);

    if (posicion == 0) {
      if (byteRecibido == 0x02) {
        posicion = 1;
      } else {
        return;
      }
    }
    else if (posicion == 1 ) {
      posicion = 2;
      buffer[0] = byteRecibido;
    }
    else if (posicion == 2) {
      posicion = 3;
      buffer[1] = byteRecibido;
    }
    else if (posicion == 3) {
      posicion = 4;
      buffer[2] = byteRecibido;
    }
    else if (posicion == 4) {
      posicion = 5;
      comandoRecibido = byteRecibido;
    }
    else if (posicion == 5) {
      if (byteRecibido == 0x03) {
        mostrarByte(atoi(buffer));
        posicion = 0;
      } else{
        posicion = 0;
        return;
      }
    }
 }
}

void loop() {
  recepcionRutina();

 //for (int i = 0; i <= 3; i++) {
   // Serial.print(i);
    //Serial.print(" ");
    //digitalWrite(OEPin, LOW);
    //enviarByte(i);  
    //delay(3000);

   
 // }

  //mostrarByte(5678);

  //for (int valor = 0; valor <= 9; valor++) {
    //probarDigito(valor);
    //delay(10000);
  //}
 
  //delay(50);
}
