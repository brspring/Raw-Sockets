## Comunicação Raw Sockets

Implementar um streaming de vídeo baseado em um protocolo próprio, baseado no protocolo Kermit. Não se trata estritamente de streaming, pois não acontece uma transmissão contínua com visualização progressiva.
O sistema desenvolvido consiste em dois elementos, um cliente e um servidor. 

O servidor e o cliente devem executar em máquinas distintas, conectadas por um cabo de rede. 

Uma vez conectados e iniciados cliente e servidor, no cliente é mostrada uma lista de títulos de vídeos disponíveis no servidor, em mp4. O servidor possui uma pasta 'filmes' que possui uma lista de videos a serem transmitidas ao cliente. 

Quando o Cliente solicita o download de um video o servidor manda esse video em frames de 63 bytes até o vídeo completo ser enviado ao cliente. Ao receber o video completo o cliente abre o celluloid automaticamente com o video que foi solicitado.

![image](https://github.com/user-attachments/assets/62a66824-fecc-4f94-84c4-835dc5388ef4)


