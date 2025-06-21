#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include<Wire.h>
#include<LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

typedef struct node {
    uint8_t * data;
    int priority;
    struct node * next;
} LLNode;

typedef struct {
    LLNode * head;
} LL;



#define TRUE 1
#define FALSE 0

LL * create_queue() {
    LL * temp = (LL*)malloc(sizeof(LL));
    if (!temp) return NULL;
    temp->head = NULL;
    return temp;
}

// Adds a packet to the queue sorted by priority (higher priority first)
void add_packet(LL * list, uint8_t * newData, int priority) {
    if (!list) return;

    LLNode * node = (LLNode*)malloc(sizeof(LLNode));
    if (!node) return;

    node->data = (uint8_t *)newData;
    node->priority = priority;
    node->next = NULL;

    // If the list is empty or new node has higher priority than head
    if (list->head == NULL || priority > list->head->priority) {
        node->next = list->head;
        list->head = node;
    } else {
        LLNode * current = list->head;
        // Find insertion point
        while (current->next != NULL && current->next->priority >= priority) {
            current = current->next;
        }
        node->next = current->next;
        current->next = node;
    }
}

// Return and remove the highest-priority packet (at head)
LLNode * return_packet(LL * list) {
    if (!list || !list->head) return NULL;

    LLNode * node = list->head;
    LLNode * node_copy = node;

    list->head = node->next;
    //free(node);
    return node_copy;
}

// Frees the entire linked list
void freeLL(LL * list) {
    if (!list) return;

    LLNode * current = list->head;
    LLNode * temp;

    while (current != NULL) {
        temp = current->next;
        free(current);
        current = temp;
    }
    free(list);
}

void print_packets(LL * list) {
    if (!list || !list->head) {
        Serial.println("Queue is empty.\n");
        return;
    }

    LLNode * current = list->head;
    Serial.println("Packets in queue (highest priority first):\n");
    while (current != NULL) {
        Serial.print("Priority: ");
        Serial.println(current->priority);

        Serial.print("Data: ");
        for (int i = 0; i < 3; i++) {
            Serial.print(current->data[i]);
            Serial.print(" ");
        }
        Serial.println();
        current = current->next;
    }
}

LL * list;
LLNode * node;

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Hello Indi!");

  static uint8_t data1[3] = {23, 56, 22};
  static uint8_t data2[3] = {13, 36, 82};
  static uint8_t data3[3] = {3, 44, 25};
  static uint8_t data4[3] = {1, 3, 2};

  list = create_queue();
  add_packet(list, data1, 1);
  add_packet(list, data2, 3);
  add_packet(list, data3, 2);
  add_packet(list, data4, 1);

  print_packets(list);

  for (int _ = 0; _ < 3; _++) {
    node = return_packet(list);
    for (int i = 0; i < 3; i++) {
      lcd.clear();
      // Line 1
      lcd.setCursor(0,0);
      lcd.print("Data #");
      lcd.print(i+1);
      lcd.print(": ");
      lcd.print((node->data)[i]);
      // Line 2
      lcd.setCursor(0,1);
      lcd.print("Priority: ");
      lcd.print(node->priority);
      delay(1000);
    }
  }
}

void loop() {

}