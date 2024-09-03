// Configuración de la red WiFi
char* WIFI_SSID = "Nombre de la red"; //Nombre de la red
char* WIFI_PASSWORD = "Contraseña del WiFi"; //Contraseña del WiFi

// Configuración de Telegram
String botToken = "Token de API del bot de Telegram";  // Token de API del bot de Telegram
String chatID = "chat ID - proporcionado por Telegram";         // chat ID - proporcionado por Telegram

//Email:  Configuración del servidor SMTP
char AUTHOR_EMAIL[50] = "email emisor";  // Asigna un tamaño suficiente
char AUTHOR_PASSWORD[50] = "codntraseña de aplicación";     // Asigna un tamaño suficiente
char RECIPIENT_EMAIL[50] = "email receptor";       // Asigna un tamaño suficiente

/*
TELEGRAM - Token de API del bot de Telegram
Paso 1: Abre la aplicación de Telegram
Abre la aplicación de Telegram en tu dispositivo móvil o en la versión web/escritorio.
Asegúrate de estar iniciado en tu cuenta de Telegram.
Paso 2: Buscar el bot "BotFather"
En el campo de búsqueda de Telegram, escribe @BotFather.
Selecciona el perfil oficial de BotFather. Este es el bot oficial de Telegram para gestionar y crear otros bots.
Paso 3: Iniciar una conversación con BotFather
Haz clic en Iniciar o en Start para comenzar una conversación con BotFather.
BotFather te proporcionará una lista de comandos que puedes usar.
Paso 4: Crear un nuevo bot
Escribe o selecciona el comando /newbot y presiona Enter.
BotFather te pedirá que le des un nombre a tu bot. Este es el nombre público que los usuarios verán en Telegram. Escribe el nombre que deseas.
Luego, te pedirá que elijas un nombre de usuario para tu bot. Este nombre debe terminar con "bot". Por ejemplo, MiSuperBot o MiSuperBot_bot.
Si el nombre de usuario que elegiste ya está en uso, deberás intentar con otro hasta que sea aceptado.
Paso 5: Obtén tu Token de API
Una vez que hayas creado tu bot con éxito, BotFather te enviará un mensaje que incluye el Token de API.
El Token de API es una cadena de caracteres única que permite a tu aplicación comunicarse con la API de Telegram en nombre de tu bot.
Este token tendrá un formato similar a este: 123456789:ABCdefGHIjklMNO_pqrSTUvWXYz

TELEGRAM chat ID
Paso 1: Iniciar un chat con tu bot
Abre Telegram en tu dispositivo móvil o en la versión web/escritorio.
Busca el bot que creaste anteriormente usando su nombre de usuario. Este será algo como @TuBotUsuario.
Inicia una conversación con tu bot haciendo clic en su nombre y luego en Iniciar o Start.
Paso 2: Enviar un mensaje al bot
Envía cualquier mensaje al bot (puede ser un simple "Hola" o cualquier texto).
Esto es necesario para que el bot pueda reconocer tu Chat ID.
Paso 3: Usar una API de Telegram para obtener el Chat ID
Abre tu navegador web y en la barra de direcciones, ingresa la siguiente URL, reemplazando TOKEN con el Token de API de tu bot que obtuviste de BotFather:
https://api.telegram.org/botTOKEN/getUpdates
Por ejemplo, si tu token es 123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11, la URL sería:
https://api.telegram.org/bot123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11/getUpdates
Presiona Enter para acceder a la URL.
Paso 4: Encontrar tu Chat ID en la respuesta
La URL anterior devolverá un JSON con los últimos mensajes recibidos por tu bot.
Busca en la respuesta un bloque similar a este:
{
    "update_id": 123456789,
    "message": {
        "message_id": 1,
        "from": {
            "id": 987654321,  // Este es tu Chat ID
            "is_bot": false,
            "first_name": "TuNombre",
            "username": "TuUsuario",
            "language_code": "en"
        },
        "chat": {
            "id": 987654321,  // Este es tu Chat ID
            "first_name": "TuNombre",
            "username": "TuUsuario",
            "type": "private"
        },
        "date": 1630426908,
        "text": "Hola"
    }
}
El Chat ID es el número que aparece en el campo "id" dentro del bloque "chat". En este ejemplo, el Chat ID es 987654321.



CORREO ELECTRONICO - CONTRASEÑA DE APLICACION
Paso 0: Crea un correo gmail
Paso 1: Inicia sesión en esa cuenta de Google
Abre el navegador y ve a https://myaccount.google.com.
Inicia sesión con la cuenta de Google para la que deseas generar una contraseña de aplicación.
Paso 2: Accede a la configuración de seguridad
En la página principal de tu cuenta de Google, busca y selecciona la opción "Seguridad" en el menú de la izquierda.
Desplázate hacia abajo hasta la sección "Iniciar sesión en Google".
Paso 3: Activa la verificación en dos pasos
Si aún no lo has hecho, activa la verificación en dos pasos.
Haz clic en "Verificación en dos pasos".
Sigue las instrucciones para configurar la verificación en dos pasos (puede ser a través de un mensaje de texto, una aplicación de autenticación, etc.).
Paso 4: Genera una contraseña de aplicación
Una vez activada la verificación en dos pasos, vuelve a la página de Seguridad.
En la sección "Iniciar sesión en Google", verás la opción "Contraseñas de aplicaciones".
Haz clic en "Contraseñas de aplicaciones".

*/
