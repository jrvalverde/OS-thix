#ifndef _THIX_IRQ_H
#define _THIX_IRQ_H


#define NOIRQ           ((unsigned char)-1)

#define CASCADE_IRQ      2


int  irq_init(void);

int  irq_register(unsigned char __irq);
void irq_unregister(unsigned char __irq);


#endif  /* _THIX_IRQ_H */
