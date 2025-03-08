/******************************************************************************
* Laboration 2
*
******************************************************************************/
////////////////////////////////////////////////////////////////////////////////
// Portdefinitioner
////////////////////////////////////////////////////////////////////////////////
// GPIO Port D
#define GPIO_D_BASE 0x40020C00
#define GPIO_D_MODER ((volatile unsigned int *)(GPIO_D_BASE))
#define GPIO_D_OTYPER ((volatile unsigned short *)(GPIO_D_BASE + 0x4))
#define GPIO_D_OSPEEDR ((volatile unsigned int *)(GPIO_D_BASE + 0x8))
#define GPIO_D_PUPDR ((volatile unsigned int *)(GPIO_D_BASE + 0xC))
#define GPIO_D_IDRLOW ((volatile unsigned char *)(GPIO_D_BASE + 0x10))
#define GPIO_D_IDRHIGH ((volatile unsigned char *)(GPIO_D_BASE + 0x11))
#define GPIO_D_ODRLOW ((volatile unsigned char *)(GPIO_D_BASE + 0x14))
#define GPIO_D_ODRHIGH ((volatile unsigned char *)(GPIO_D_BASE + 0x15))

// GPIO Port E
#define GPIO_E_BASE 0x40021000
#define GPIO_E_MODER ((volatile unsigned int *)(GPIO_E_BASE))
#define GPIO_E_OTYPER ((volatile unsigned short *)(GPIO_E_BASE + 0x4))
#define GPIO_E_OSPEEDR ((volatile unsigned int *)(GPIO_E_BASE + 0x8))
#define GPIO_E_PUPDR ((volatile unsigned int *)(GPIO_E_BASE + 0xC))
#define GPIO_E_IDRHIGH ((volatile unsigned char *)(GPIO_E_BASE + 0x10 + 1))
#define GPIO_E_ODRLOW ((volatile unsigned char *)(GPIO_E_BASE + 0x14))
#define GPIO_E_ODRHIGH ((volatile unsigned char *)(GPIO_E_BASE + 0x14 + 1))

// SysTick
#define STK_CTRL ((volatile unsigned int *)(0xE000E010))
#define STK_LOAD ((volatile unsigned int *)(0xE000E014))
#define STK_VAL ((volatile unsigned int *)(0xE000E018))

////////////////////////////////////////////////////////////////////////////////
// Blockerande delay med SysTick
////////////////////////////////////////////////////////////////////////////////
void delay_250ns(void) {
    /* SystemCoreClock = 168000000 */
    *STK_CTRL = 0;
    *STK_LOAD = ((168 / 4) - 1);
    *STK_VAL = 0;
    *STK_CTRL = 5;
    while ((*STK_CTRL & 0x10000) == 0);
    *STK_CTRL = 0;
}

void delay_micro(unsigned int us) {
    for (unsigned int i = 0; i < us; i++) {
        delay_250ns();
        delay_250ns();
        delay_250ns();
        delay_250ns();
    }
}

void delay_milli(unsigned int ms) {
    for (unsigned int i = 0; i < ms; i++)
        delay_micro(1000);
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard
////////////////////////////////////////////////////////////////////////////////
// Initialisera Port D för tangentborder, inkopplat på den övre
// delen av porten (b15-b8)
void init_gpio_keyboard() {
    // b15-b12 used for output to rows
    // b11-b8 used for input from columns
    *GPIO_D_MODER = 0x00555555;
    // Pinnarna som läses från tangentbordet är spänningssatta om
    // nedtryckta och flytande annars, så behöver Pull Down
    *GPIO_D_PUPDR = 0x00AA0000;
    // Pinnarna som väljer rad skall vara spänningssatta (Push/Pull)
    *GPIO_D_OTYPER = 0x00000000;
    *GPIO_D_OSPEEDR = 0x00000000;
    *GPIO_D_ODRLOW = 0;
    *GPIO_D_ODRHIGH = 0;
}

// Aktivera en rad för läsning
void kbdActivate(unsigned int row) {
    switch (row) {
        case 1: *GPIO_D_ODRHIGH = 0x10; break;
        case 2: *GPIO_D_ODRHIGH = 0x20; break;
        case 3: *GPIO_D_ODRHIGH = 0x40; break;
        case 4: *GPIO_D_ODRHIGH = 0x80; break;
        case 0: *GPIO_D_ODRHIGH = 0x00; break;
    }
}

// Läs en rad och returnera vilken kolumn som är ett
// (antar endast en tangent nedtryckt)
int kbdGetCol(void) {
    unsigned short c;
    c = *GPIO_D_IDRHIGH;
    if (c & 0x8) return 4;
    if (c & 0x4) return 3;
    if (c & 0x2) return 2;
    if (c & 0x1) return 1;
    return 0;
}

// Returnera en byte som motsvarar den knapp som är nedtryckt,
// eller 0xFF om ingen knapp är nedtryckt
unsigned char keyb(void) {
    unsigned char key[] = {
        '1', '2', '3', 'A',
        '4', '5', '6', 'B',
        '7', '8', '9', 'C',
        'E', '0', 'F', 'D'
    };
    for (int row = 1; row <= 4; row++) {
        kbdActivate(row);
        delay_250ns();
        int col = kbdGetCol();
        if (col) {
            return key[4 * (row - 1) + (col - 1)];
        }
    }
    *GPIO_D_ODRHIGH = 0;
    return 0xFF;
}

////////////////////////////////////////////////////////////////////////////////
// ASCII display
////////////////////////////////////////////////////////////////////////////////
// Konfigurera de GPIO portar vi använder för ASCII displayen
void init_gpio_ascii() {
    *GPIO_E_MODER = 0x55000000; /* all bits outputs */
    *GPIO_E_OTYPER = 0x00000000; /* outputs are push/pull */
    *GPIO_E_OSPEEDR = 0x55555555; /* medium speed */
    *GPIO_E_PUPDR = 0x55550000; /* inputs are pull up */
}

// Hjälp-defines
#define B_E 0x40
#define B_SELECT 4
#define B_RW 2
#define B_RS 1
#define ascii_ctrl_bit_set(x) *GPIO_E_ODRLOW = (B_SELECT | (x) | *GPIO_E_ODRLOW)
#define ascii_ctrl_bit_clear(x) *GPIO_E_ODRLOW = (B_SELECT | (*GPIO_E_ODRLOW & ~(x)))

void ascii_write_controller(unsigned char c) {
    // Vi räknar med att tsu1 = 40ns (ca 6 clockcykler) passerat sedan RS eller RW ändrades
    // (lite modigt, om en optimerande kompilator inlinat koden)
    ascii_ctrl_bit_set(B_E);
    *GPIO_E_ODRHIGH = c;
    // Vi måste vänta tsu2 = 80ns innan vi släcker E efter att ha lagt ut data
    // Men vi har också att E måste varit aktiv i minst 230ns, så
    delay_250ns();
    ascii_ctrl_bit_clear(B_E);
    // Efter negativ E flank måste vi vänta i 10ns (~2 cykler, försummbart)
    // innan vi ändrar RS eller RW, men E måste vara låg i 230ns innan den
    // går hög igen, så vi väntar 250 ns.
    delay_250ns();
}

void ascii_write_cmd(unsigned char c) {
    ascii_ctrl_bit_clear(B_RS | B_RW);
    ascii_write_controller(c);
}

void ascii_write_data(unsigned char c) {
    ascii_ctrl_bit_clear(B_RW);
    ascii_ctrl_bit_set(B_RS);
    ascii_write_controller(c);
}

unsigned char ascii_read_controller(void) {
    // Vi räknar med att tsu = 60ns (ca 10 clockcykler) passerat sedan RS eller RW ändrades
    // (lite modigt, om en optimerande kompilator inlinat koden)
    ascii_ctrl_bit_set(B_E);
    // Det får lov att ta tD = 360ns innan datan ligger ute, så vi väntar 500ns
    delay_250ns();
    delay_250ns();
    // Och läser sedan datan
    unsigned char c = *GPIO_E_IDRHIGH;
    // Vi måste hålla E aktiv i minst tw = 450ns, men vi har redan väntat 500.
    ascii_ctrl_bit_clear(B_E);
    // Efter negativ flank måste vi vänta 10ns (~2 clockcykler, försummbart)
    return c;
}

unsigned char ascii_read_status() {
    // Porten är normalt inställd för att skriva data
    // Vi väljer att bara ändra på detta när vi ibland måste göra en läsning
    *GPIO_E_MODER = 0x00005555; // b15-8 are inputs, 7-0 are outputs
    // Förbered för läsning av status
    ascii_ctrl_bit_set(B_RW);
    ascii_ctrl_bit_clear(B_RS);
    // Läs från statusregistret
    unsigned char c = ascii_read_controller();
    // Återställ porten
    *GPIO_E_MODER = 0x55555555; // all bits outputs
    return c;
}

void ascii_wait_ready(void) {
    while ((ascii_read_status() & 0x80) == 0x80);
    delay_micro(8); // Måste vänta 8us efter att biten gått låg
}

void ascii_write_char(unsigned char c) {
    // För säkerhets skull
    ascii_wait_ready();
    ascii_write_data(c);
    // Kommandospecifik fördröjning
    ascii_wait_ready();
}

void ascii_init() {
    ascii_wait_ready(); // För säkerhets skull
    ascii_write_cmd(0x38); // Function set
    ascii_wait_ready();
    ascii_write_cmd(0x0C); // Display on
    ascii_wait_ready();
    ascii_write_cmd(1); // Clear display
    ascii_wait_ready();
    ascii_write_cmd(6); // Entry mode set
    ascii_wait_ready();
}

void ascii_gotoxy(unsigned char x, unsigned char y) {
    // Addressen är yyxxxxxx
    // Vi använder (1,1) är översta vänstra hörnet
    unsigned char address = x - 1;
    address |= (y == 0) << 4;
    ascii_write_cmd(0x80 | address);
}

void init_app(void) {

    // starta klockor port D och E
    
    *((unsigned long *)0x40023830) = 0x18;
    // initiera PLL
    __asm volatile("LDR R0,=0x08000209\n BLX R0 \n"); // SUSSy ass unpredictable 
    init_gpio_keyboard();
    init_gpio_ascii();
}

void main(void) {
    // Konfigurera GPIO portar
    init_app();

    // Test that upper four pins are output, on bargraph
    *GPIO_D_ODRHIGH = 0xA0;
    *GPIO_D_ODRHIGH = 0x5F;

    // Test that lower four pins are inputs, using dipswitch
    __attribute__((unused)) unsigned char test = 0x0F & *GPIO_D_IDRHIGH;

    // Initiera ASCII displayen
    ascii_init();
    // Kör för alltid
    while (1) {
        // Flytta tillbaka markören när vi nått slutet på raden
        ascii_gotoxy(1, 1);
        for (int x = 0; x < 20; x++) {
            // Invänta knapptryckning
            int c;
            do {
                c = keyb();
            } while (c == 0xFF);
            // Skriv ut tecknet
            ascii_write_char(c);
            // Kort delay så vi inte får för många tecken
            delay_milli(100);
        }
    }
}