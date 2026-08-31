# Projeto IoT 2 - Rede Mesh LoRa Descentralizada

Este repositório contém o código-fonte, configurações de infraestrutura e documentação para o projeto da disciplina de Internet das Coisas 2. O objetivo é estabelecer uma rede sem fio descentralizada, escalável, segura e anônima para envio de mensagens assíncronas via tecnologia LoRa.

## Estrutura do Projeto

*   **`docs/`**: Contém a apresentação do projeto, esquemas de ligação de hardware e fluxogramas do caminho dos dados.
*   **`libs/`**: Bibliotecas base do sistema.
    *   `lib_mesh_c/`: Core de criptografia, compressão e gerenciamento de blocos em C.
    *   `lib_mesh_dart/`: Integração Bluetooth e operações IPFS para a interface.
*   **`firmware/`**: Código embarcado para os nós da rede.
    *   `esp32_node/`: Projeto base para o Heltec ESP32 LoRa V3 (com suporte a display OLED e Cartão SD).
*   **`interfaces/`**: Aplicações de interação com o usuário.
    *   `app_usuario/`: Aplicativo Flutter para comunicação com o nó via Bluetooth.
*   **`servidor_aws/`**: Configurações de infraestrutura em nuvem.
    *   `ipfs_node/`: Scripts para provisionar e gerenciar o nó IPFS na AWS.

## Como Iniciar

1.  **Nuvem (AWS/IPFS):** Suba o servidor utilizando as instruções em `servidor_aws/`.
2.  **Hardware:** Compile e faça o upload do código contido em `firmware/esp32_node/` para as placas ESP32.
3.  **Interface:** Configure as bibliotecas e inicie o ambiente Flutter em `interfaces/app_usuario/`.

---
*Projeto desenvolvido para a disciplina Internet das Coisas 2.*
