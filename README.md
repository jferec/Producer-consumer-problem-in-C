# Producer-consumer-problem-in-C
SOI lab3 

Bufor 9-elementowy FIFO. Jest jeden producent i trzech konsumentów (A, B, C). 
Producent produkuje jeden element, jeżeli jest miejsce w buforze. 
Element jest usuwany z bufora, jeżeli zostanie przeczytany przez albo obu konsumentów A i B, 
albo przez obu konsumentów B i C. Konsument A nie może przeczytać elementu, 
jeżeli został on już przez niego wcześniej przeczytany, albo został przeczytany przez konsumenta C i na odwrót.

9 element FIFO buffer. There is one producer and three consumers (A, B, C).
Producer produces one element, if there is a free slot in the buffer.
Element is being deleted from the buffer, if it has been read either by both consumers A and B or B and C.
Consumer A must not read an element, if it has been already read by C and the other way round.
