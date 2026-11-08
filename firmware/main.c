#include "xparameters.h"
#include "xuartps.h"
#include "xadcps.h"
#include "xil_printf.h"
#include "platform.h"
#include "sleep.h"
#include <stdio.h>
#include <string.h>

// define uri pt UART0/1 DEVICE_ID si BASEADDR
#ifndef SDT
 #define UART_ESP_DEVICE_ID XPAR_XUARTPS_0_DEVICE_ID
 #define UART_PC_DEVICE_ID XPAR_XUARTPS_1_DEVICE_ID
#else
 #define UART_ESP_BASEADDRESS XPAR_XUARTPS_0_BASEADDR
 #define UART_PC_BASEADDRESS XPAR_XUARTPS_1_BASEADDR
#endif

// define uri XADC DEVICE_ID
#ifndef SDT
#define XADC_DEVICE_ID XPAR_XADCPS_0_DEVICE_ID
#else
#define XADC_DEVICE_ID 0
#endif

#define BUFFER_SIZE 2048
#define UART_CHUNK_SIZE 64
#define BAUD_RATE 115200

// canale XADC
#define XADC_AUX_CH6 22   // canal 6
#define XADC_AUX_CH7 23   // canal 7
#define XADC_AUX_CH14 30  // canal 14

// define uri masca canale
#define XADCPS_SEQ_CH_VAUX6  (1 << 22)
#define XADCPS_SEQ_CH_VAUX7  (1 << 23)
#define XADCPS_SEQ_CH_VAUX14 (1 << 30)

#define LSB_BITS_NOT_USED 4
#define VOLT_CONV_FACTOR 3.0/4096.0

// instante UART uri si XADC
XUartPs Uart_ESP;
XUartPs Uart_PC;
XAdcPs XAdcInst;

static char sendBuffer[BUFFER_SIZE]; // buffer trimitere date
static char receiveBuffer[BUFFER_SIZE]; // buffer receptie date

// valori stocare citire potentiometre
u32 senzor1 = 0;
u32 senzor2 = 0;
u32 senzor3 = 0;

// html interfata web cu auto-refresh la 2 secunde
const char INTERFATA_HTML[] = 
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=utf-8\r\n"
"Connection: close\r\n\r\n"
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta http-equiv='refresh' content='2'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Monitorizare v2</title><style>"
"body{background:#f0f4f8;font-family:Arial;text-align:center;padding:20px;}"
".sensor{background:#fff;border-radius:8px;padding:15px;margin:10px auto;max-width:300px;border:1px solid #ccc;box-shadow:0 2px 5px rgba(0,0,0,0.1);}"
"h2{color:#333;font-size:1.2rem;margin-bottom:5px;}"
".val{font-size:2.5rem;font-weight:bold;margin:10px 0;}"
".st{font-weight:bold;font-size:1.1rem;}"
"</style></head><body>"
"<h1>Monitorizare Simplificata</h1>"
// date potentiometru 1
"<div class='sensor'><h2>Potentiometru 1 (CH6)</h2>"
"<div class='val' style='color:%s'>%.2f V</div>"
"<div class='st' style='color:%s'>%s</div></div>"
// date potentiometru 2
"<div class='sensor'><h2>Potentiometru 2 (CH7)</h2>"
"<div class='val' style='color:%s'>%.2f V</div>"
"<div class='st' style='color:%s'>%s</div></div>"
// date potentiometru 3
"<div class='sensor'><h2>Potentiometru 3 (CH14))</h2>"
"<div class='val' style='color:%s'>%.2f V</div>"
"<div class='st' style='color:%s'>%s</div></div>"
// footer
"<div style='margin-top:20px;color:#999'>Auto-Refresh 2s</div></body></html>";

// functie trimitere chunk uri de 64 biti catre ESP01
void XUART_ESP_Send(const char* data) {
    int len = strlen(data);
    int sent = 0;
    
    while (sent < len) {
        int chunk_size = 0;
        if (len - sent > UART_CHUNK_SIZE){
            chunk_size = UART_CHUNK_SIZE;
        } else {
            chunk_size = len - sent;
        }
        
        for (int i = 0; i < chunk_size; i++) {
            while (XUartPs_IsTransmitFull(Uart_ESP.Config.BaseAddress));
            XUartPs_SendByte(Uart_ESP.Config.BaseAddress, data[sent + i]);
        }
        
        sent += chunk_size;
        
        if (sent < len) {
            usleep(2000); // delay pt siguranta trimitere date corect
        }
    }
}

// functie trimitere chunk uri de 64 biti catre hTerm (PC)
void XUART_PC_Send(const char* data) {
    int len = strlen(data);
    int sent = 0;
    
    while (sent < len) {
        int chunk_size = 0;
        if (len - sent > UART_CHUNK_SIZE){
            chunk_size = UART_CHUNK_SIZE;
        } else {
            chunk_size = len - sent;
        }
        
        for (int i = 0; i < chunk_size; i++) {
            while (XUartPs_IsTransmitFull(Uart_PC.Config.BaseAddress));
            XUartPs_SendByte(Uart_PC.Config.BaseAddress, data[sent + i]);
        }
        
        sent += chunk_size;
        
        if (sent < len) {
            usleep(2000);   // delay pt siguranta trimitere date corect
        }
    }
}

// initializare XADC
int InitializeXAdc(XAdcPs *XAdcInstPtr) {
    int Status;
    XAdcPs_Config *ConfigPtr;
    
    XUART_PC_Send("\r\n=== Initializare XADC ===\r\n");
    
    ConfigPtr = XAdcPs_LookupConfig(XADC_DEVICE_ID);
    if (ConfigPtr == NULL) {
        XUART_PC_Send("Eroare configurare XADC !\r\n");
        return XST_FAILURE;
    }
    
    Status = XAdcPs_CfgInitialize(XAdcInstPtr, ConfigPtr, ConfigPtr->BaseAddress);
    if (Status != XST_SUCCESS) {
        XUART_PC_Send("Eroare initializare XADC !\r\n");
        return XST_FAILURE;
    }
    
    Status = XAdcPs_SelfTest(XAdcInstPtr);
    if (Status != XST_SUCCESS) {
        XUART_PC_Send("Eroare selfTest XADC !\r\n");
        return XST_FAILURE;
    }
    
    XAdcPs_SetSequencerMode(XAdcInstPtr, XADCPS_SEQ_MODE_CONTINPASS);

    // activare canale pt citire
    u32 channelMask = XADCPS_SEQ_CH_VAUX6 | XADCPS_SEQ_CH_VAUX7 | XADCPS_SEQ_CH_VAUX14;
    XAdcPs_SetSeqChEnables(XAdcInstPtr, channelMask);

    XUART_PC_Send("XADC OK!\r\n");
    return XST_SUCCESS;
}

// struct pt status tensiune
typedef struct {
    const char* text;
    const char* color;
} StatusInfo;

StatusInfo getVoltageStatus(float voltage) {
    StatusInfo status;
    
    if (voltage > 2.8 || voltage < 0.5) {
        status.text = "VALOARE CRITICA";
        status.color = "#e53e3e";
    } else if (voltage > 2.5 || voltage < 0.8) {
        status.text = "ATENTIE";
        status.color = "#d69e2e";
    } else {
        status.text = "VALOARE NORMALA";
        status.color = "#48bb78";
    }
    
    return status;
}

// trimitere comanda catre ESP
void ESP_SendCommand(const char* cmd) {
    // curatare buffer de primire
    u32 receivedData = XUartPs_Recv(&Uart_ESP, (u8*)receiveBuffer, BUFFER_SIZE - 1);
    if (receivedData > 0) {
        receiveBuffer[receivedData] = '\0';
    }
    
    snprintf(sendBuffer, 128, "trimitere catre ESP: %s\r\n", cmd);
    XUART_PC_Send(sendBuffer);
    XUART_ESP_Send(cmd);
    XUART_ESP_Send("\r\n");
}

// primire raspuns de la ESP
void ESP_ReadResponse(int delay_ms) {
    usleep(delay_ms * 1000);
    u32 receivedData = XUartPs_Recv(&Uart_ESP, (u8*)receiveBuffer, BUFFER_SIZE - 1);
    
    if(receivedData > 0){
        receiveBuffer[receivedData] = '\0';
        snprintf(sendBuffer, BUFFER_SIZE, "primit de la ESP: %s\r\n", receiveBuffer);
        XUART_PC_Send(sendBuffer);
    }
}

// configurare ESP 
void setupESP() {
    XUART_PC_Send("\r\n=== Configurare ESP01 ===\r\n");
    
    ESP_SendCommand("AT+RST");  // comanda RST
    ESP_ReadResponse(5000);
    
    ESP_SendCommand("AT");  // test comunicare ESP
    ESP_ReadResponse(2000);
    
    ESP_SendCommand("AT+CWMODE=2");  // setare ESP ca AP (1 => STA, 2 => AP, 3 => STA & AP)
    ESP_ReadResponse(2000);
    
    ESP_SendCommand("AT+CWSAP=\"Monitorizare_v2\",\"potentiometru\",5,3");  // setare SSID si PASS
    ESP_ReadResponse(2000);
    
    ESP_SendCommand("AT+CIPMUX=1"); // permitere conexiuni multiple la ESP
    ESP_ReadResponse(2000);
    
    ESP_SendCommand("AT+CIPSERVER=1,80");  // pornire server pe port 80
    ESP_ReadResponse(2000);
    
    XUART_PC_Send("\n=== ESP01 Configurat ! ===\r\n");
    XUART_PC_Send("WiFi: Monitorizare_v2\r\n");
    XUART_PC_Send("password: potentiometru\r\n");
    XUART_PC_Send("acceseaza: http://192.168.4.1\r\n\r\n");
}

// functie trimitere pagina html cu valorile citite de la senzori
void sendHTMLPage() {
    XUART_PC_Send("\r\n=== HTTP REQUEST ===\r\n");
    
    // conversie valori citite in volti
    float v1 = (senzor1 >> LSB_BITS_NOT_USED) * VOLT_CONV_FACTOR;
    float v2 = (senzor2 >> LSB_BITS_NOT_USED) * VOLT_CONV_FACTOR;
    float v3 = (senzor3 >> LSB_BITS_NOT_USED) * VOLT_CONV_FACTOR;
    
    // obtinere status potentiometre
    StatusInfo s1 = getVoltageStatus(v1);
    StatusInfo s2 = getVoltageStatus(v2);
    StatusInfo s3 = getVoltageStatus(v3);
    
    char html[2048];
    
    snprintf(html, sizeof(html), INTERFATA_HTML,
        s1.color, v1, s1.color, s1.text,    // (culoare, valoare) ; (culoare, mesaj) 
        s2.color, v2, s2.color, s2.text,
        s3.color, v3, s3.color, s3.text
    );
    
    int html_len = strlen(html);
    
    snprintf(sendBuffer, 64, "AT+CIPSEND=0,%d\r\n", html_len);  // cati biti vom trimite catre ESP
    XUART_ESP_Send(sendBuffer);
    
    usleep(200000); // delay mic pt procesare comanda

    XUART_ESP_Send(html); // trimitem interfata

    usleep(500000); // delay putin mai mare 

    XUART_ESP_Send("AT+CIPCLOSE=0\r\n"); // inchidere conexiune

    usleep(100000);
    
    XUART_PC_Send("Interfata HTML trimisa !\r\n");
}

// functie verificare request
int checkHTTPRequest() {
    u32 receivedData = XUartPs_Recv(&Uart_ESP, (u8*)receiveBuffer, BUFFER_SIZE - 1);
    
    if (receivedData > 0) {
        receiveBuffer[receivedData] = '\0';
        
        // avem request => trimitem interfata
        if (strstr(receiveBuffer, "+IPD") != NULL) {
             return 1;
        }
    }
    return 0; 
}

int main(void)
{
    int Status;
    XUartPs_Config *ConfigESP;
    XUartPs_Config *ConfigPC;

    init_platform();
    sleep(1); 

    // initializare UART0 => ESP01
#ifndef SDT
    ConfigESP = XUartPs_LookupConfig(UART_ESP_DEVICE_ID);
#else
    ConfigESP = XUartPs_LookupConfig(UART_ESP_BASEADDRESS);
#endif
    if (NULL == ConfigESP) return XST_FAILURE;

    Status = XUartPs_CfgInitialize(&Uart_ESP, ConfigESP, ConfigESP->BaseAddress);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    Status = XUartPs_SetBaudRate(&Uart_ESP, BAUD_RATE);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    XUartPs_SetOperMode(&Uart_ESP, XUARTPS_OPER_MODE_NORMAL);


    // initializare UART1 => hTerm (PC)
#ifndef SDT
    ConfigPC = XUartPs_LookupConfig(UART_PC_DEVICE_ID);
#else
    ConfigPC = XUartPs_LookupConfig(UART_PC_BASEADDRESS);
#endif
    if (NULL == ConfigPC) return XST_FAILURE;

    Status = XUartPs_CfgInitialize(&Uart_PC, ConfigPC, ConfigPC->BaseAddress);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    Status = XUartPs_SetBaudRate(&Uart_PC, BAUD_RATE);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    XUartPs_SetOperMode(&Uart_PC, XUARTPS_OPER_MODE_NORMAL);

    XUART_PC_Send("=== Initializare Sistem ===\r\n");
    XUART_PC_Send("UART0 si UART1 OK !\r\n");

    // initializare XADC
    Status = InitializeXAdc(&XAdcInst);
    if (Status != XST_SUCCESS) {
        XUART_PC_Send("Eroare initializare XADC !\r\n");
        return XST_FAILURE;
    }

    // configurare ESP01
    setupESP();

    XUART_PC_Send("\r\n=== bucla citire ===\r\n");
    
    int cnt_afisare = 0;
    int prag_afisare_hterm = 5000; // afisare periodica in hTerm

    while (1) {

        senzor1 = XAdcPs_GetAdcData(&XAdcInst, XADC_AUX_CH6); 
        senzor2 = XAdcPs_GetAdcData(&XAdcInst, XADC_AUX_CH7);  
        senzor3 = XAdcPs_GetAdcData(&XAdcInst, XADC_AUX_CH14);  

        if (checkHTTPRequest()) {
            sendHTMLPage();
            // curatam bufferul dupa ce am trimis interfata
            while(XUartPs_Recv(&Uart_ESP, (u8*)receiveBuffer, BUFFER_SIZE));
        }
        
        cnt_afisare++;
        if(cnt_afisare >= prag_afisare_hterm){

            float v1 = (senzor1 >> LSB_BITS_NOT_USED) * VOLT_CONV_FACTOR;
            float v2 = (senzor2 >> LSB_BITS_NOT_USED) * VOLT_CONV_FACTOR;
            float v3 = (senzor3 >> LSB_BITS_NOT_USED) * VOLT_CONV_FACTOR;
            
            snprintf(sendBuffer, BUFFER_SIZE, "Valori XADC: %.3fV, %.3fV, %.3fV\r\n", v1, v2, v3);
            XUART_PC_Send(sendBuffer);
            cnt_afisare = 0;
        }
        
        usleep(1000);
    }

    cleanup_platform();
    return XST_SUCCESS;
}