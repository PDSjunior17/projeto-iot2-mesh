const pptxgen = require('pptxgenjs');

let pres = new pptxgen();
pres.layout = 'LAYOUT_16x9';
pres.author = 'Paulo';
pres.title = 'Projeto IoT 2: Rede Descentralizada LoRa com IPFS';

// Slide 1: Title
let slide = pres.addSlide();
slide.addText('Projeto IoT 2', { x: 0.5, y: 0.5, fontSize: 48, color: '000080', align: 'center' });
slide.addText('Rede Descentralizada LoRa com IPFS', { x: 0.5, y: 1.2, fontSize: 36, color: '000080', align: 'center' });
slide.addText('Disciplina IoT 2', { x: 0.5, y: 2.0, fontSize: 24, color: '555555', align: 'center' });
slide.addText('Paulo', { x: 0.5, y: 2.8, fontSize: 18, color: '777777', align: 'center' });

// Slide 2: Objetivo
slide = pres.addSlide();
slide.addText('Objetivo', { x: 0.5, y: 0.3, fontSize: 32, color: '006400', bold: true });
slide.addText([
    { text: '• Rede sem fio descentralizada, escalável, segura e anônima', options: { breakLine: true } },
    { text: '• Envio de mensagens assíncronas', options: { breakLine: true } },
    { text: '• Tecnologia LoRa como camada física', options: { breakLine: true } }
], { x: 0.7, y: 0.8, fontSize: 20, color: '000000' });

// Slide 3: Componentes Físicos
slide = pres.addSlide();
slide.addText('Componentes Físicos', { x: 0.5, y: 0.3, fontSize: 32, color: '006400', bold: true });
slide.addText([
    { text: '• LoRa – modulação de longo alcance, baixo consumo', options: { breakLine: true } },
    { text: '• ESP32 LoRa V3 – microcontrolador com Bluetooth/WiFi integrado', options: { breakLine: true } },
    { text: '• Módulo e cartão SD – armazenamento persistente para logs e arquivos', options: { breakLine: true } },
    { text: '• Dispositivo do usuário – celular, computador, notebook, SBC ou microcontrolador', options: { breakLine: true } }
], { x: 0.7, y: 0.8, fontSize: 20, color: '000000' });

// Slide 4: Componentes Lógicos
slide = pres.addSlide();
slide.addText('Componentes Lógicos', { x: 0.5, y: 0.3, fontSize: 32, color: '006400', bold: true });
slide.addText([
    { text: '• Bibliotecas (C e Dart/Flutter)', options: { breakLine: true } },
    { text: '  – Criptografia, compressão, divisão em blocos', options: { breakLine: true } },
    { text: '  – Conexão Bluetooth, envio/download IPFS', options: { breakLine: true } },
    { text: '• IPFS – armazenamento público e descentralizado de arquivos', options: { breakLine: true } },
    { text: '• Servidor AWS – nó IPFS na nuvem, pinagem de arquivos', options: { breakLine: true } },
    { text: '• Interface do Usuário – app desktop/mobile ou código para microcontrolador', options: { breakLine: true } },
    { text: '• Aviso – quatro tipos (envio, hash IPFS, bloco com interferência, aviso com interferência)', options: { breakLine: true } },
    { text: '• Mensagem – comprimida, criptografada, dividida em blocos, CRC‑16 LoRa', options: { breakLine: true } }
], { x: 0.7, y: 0.8, fontSize: 18, color: '000000' });

// Slide 5: Caminho de Dados
slide = pres.addSlide();
slide.addText('Caminho de Dados', { x: 0.5, y: 0.3, fontSize: 32, color: '006400', bold: true });
slide.addText([
    { text: '• Arquivo Pequeno – mensagens de texto', options: { breakLine: true } },
    { text: '• Arquivo Grande – arquivos via AWS IPFS (hash)', options: { breakLine: true } }
], { x: 0.7, y: 0.8, fontSize: 20, color: '000000' });

// Slide 6: Fluxo de Arquivo Pequeno (parte 1)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Pequeno', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '1. Usuário:', options: { breakLine: true } },
    { text: '   – Comprime a mensagem', options: { breakLine: true } },
    { text: '   – Criptografa com chave pública do destinatário', options: { breakLine: true } },
    { text: '   – Divide em blocos respeitando limite LoRa', options: { breakLine: true } }
], { x: 0.6, y: 0.6, fontSize: 18, color: '000000' });
slide.addText([
    { text: '2. Cria aviso de envio:', options: { breakLine: true } },
    { text: '   – Cabeçalho: id_origem/id_destino/n/flag_recebimento/código_aleatório/ACK', options: { breakLine: true } }
], { x: 0.6, y: 1.8, fontSize: 18, color: '000000' });

// Slide 7: Fluxo de Arquivo Pequeno (parte 2)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Pequeno (continuação)', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '3. Conecta via Bluetooth ao nó e envia aviso', options: { breakLine: true } },
    { text: '4. Nó retransmite aviso M‑1 vezes em canal específico (evita colisão)', options: { breakLine: true } },
    { text: '5. Nós que recebem aviso mudam para canal de mensagem e recebem blocos', options: { breakLine: true } },
    { text: '   – Limitam número de bloco por canal, trocam de canal ao atingir limite', options: { breakLine: true } }
], { x: 0.6, y: 0.8, fontSize: 18, color: '000000' });

// Slide 8: Fluxo de Arquivo Pequeno (parte 3)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Pequeno (verificação e armazenamento)', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '6. Nó verifica CRC‑16 LoRa em cada bloco', options: { breakLine: true } },
    { text: '   – Se erro, solicita reenvio do bloco via aviso', options: { breakLine: true } },
    { text: '7. Nó armazena mensagem completa no cartão SD', options: { breakLine: true } },
    { text: '   – Verifica se destinatário está conectado via Bluetooth', options: { breakLine: true } },
    { text: '   – Se sim, envia arquivo; se não, compartilha com nós vizinhos', options: { breakLine: true } }
], { x: 0.6, y: 0.8, fontSize: 18, color: '000000' });

// Slide 9: Fluxo de Arquivo Pequeno (parte 4)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Pequeno (confirmação e redundância)', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '8. Quando nó detecta destinatário conectado, envia mensagem', options: { breakLine: true } },
    { text: '   – Dispositivo confirma recebimento e decodificação', options: { breakLine: true } },
    { text: '   – Nó envia aviso de recebimento (flag=1) para rede', options: { breakLine: true } },
    { text: '9. Nós atualizam tabela interna de recebimento no cartão SD', options: { breakLine: true } },
    { text: '10. Quando rede ociosa, rotina compartilha arquivos entre nós para redundância', options: { breakLine: true } }
], { x: 0.6, y: 0.8, fontSize: 18, color: '000000' });

// Slide 10: Fluxo de Arquivo Pequeno (parte 5)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Pequeno (retransmissão de bloco)', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '11. Se houver erro em bloco, nó solicita reenvio via aviso', options: { breakLine: true } },
    { text: '   – Cabeçalho: id_origem/id_destino/número_do_bloco/código_aleatório/ACK', options: { breakLine: true } },
    { text: '   – Primeiro solicita aos nós mais próximos (sinal forte)', options: { breakLine: true } },
    { text: '   – Se não atender, solicita aos nós mais distantes', options: { breakLine: true } }
], { x: 0.6, y: 0.8, fontSize: 18, color: '000000' });

// Slide 11: Fluxo de Arquivo Grande (AWS IPFS)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Grande (via AWS IPFS)', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '1. Usuário envia arquivo comprimido e criptografado para nó AWS (IPFS na nuvem)', options: { breakLine: true } },
    { text: '2. Nó AWS gera hash IPFS, pinna o arquivo, retorna hash ao usuário', options: { breakLine: true } },
    { text: '3. Usuário cria aviso com hash:', options: { breakLine: true } },
    { text: '   – Cabeçalho: id_origem/id_destino/flag_recebimento/hash_IPFS/ACK', options: { breakLine: true } }
], { x: 0.6, y: 0.8, fontSize: 18, color: '000000' });

// Slide 12: Fluxo de Arquivo Grande (continuação)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Grande (continuação)', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '4. Conecta via Bluetooth ao nó e envia aviso', options: { breakLine: true } },
    { text: '5. Nó retransmite aviso M‑1 vezes, mantém canal de aviso (não muda para mensagem)', options: { breakLine: true } },
    { text: '6. Nó verifica CRC‑16 do aviso; solicita reenvio se corrompido', options: { breakLine: true } },
    { text: '7. Nó armazena aviso no cartão SD', options: { breakLine: true } },
    { text: '   – Verifica se destinatário conectado; envio ou compartilhamento com nós', options: { breakLine: true } }
], { x: 0.6, y: 0.8, fontSize: 18, color: '000000' });

// Slide 13: Fluxo de Arquivo Grande (final)
slide = pres.addSlide();
slide.addText('Fluxo de Arquivo Grande (final)', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '8. Quando nó detecta destinatário conectado, baixa arquivo da AWS usando hash e envia', options: { breakLine: true } },
    { text: '   – Dispositivo confirma recebimento, descriptografa e exibe', options: { breakLine: true } },
    { text: '   – Nó envia aviso de recebimento (flag=1) para rede', options: { breakLine: true } },
    { text: '9. Nós atualizam tabela de recebimento no cartão SD', options: { breakLine: true } },
    { text: '10. Rotina ociosa compartilha arquivos entre nós (redundância)', options: { breakLine: true } },
    { text: '11. Solicitação de reenvio de aviso (mesma lógica do arquivo pequeno)', options: { breakLine: true } }
], { x: 0.6, y: 0.8, fontSize: 18, color: '000000' });

// Slide 14: Interface do Usuário e Bibliotecas
slide = pres.addSlide();
slide.addText('Interface do Usuário e Bibliotecas', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: 'Interface do Usuário', options: { breakLine: true } },
    { text: '– Aplicativo desktop, mobile ou código para microcontrolador', options: { breakLine: true } },
    { text: '– Utiliza bibliotecas para funções de rede', options: { breakLine: true } },
    { text: '', options: { breakLine: true } },
    { text: 'Biblioteca C', options: { breakLine: true } },
    { text: '– Roda em microcontroladores e desktop/mobile via Flutter', options: { breakLine: true } },
    { text: '– Funcionalidades: criptografia, compressão, divisão de blocos, conexão Bluetooth, envio/download IPFS', options: { breakLine: true } },
    { text: '', options: { breakLine: true } },
    { text: 'Biblioteca Dart', options: { breakLine: true } },
    { text: '– Conexões desktop/mobile via Bluetooth', options: { breakLine: true } },
    { text: '– Envio/download de arquivos IPFS', options: { breakLine: true } },
    { text: '', options: { breakLine: true } },
    { text: 'Função opcional sem criptografia para testes e envio rápido', options: { breakLine: true } }
], { x: 0.6, y: 0.5, fontSize: 18, color: '000000' });

// Slide 15: Cronograma (proposto)
slide = pres.addSlide();
slide.addText('Cronograma Proposto', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '1. Pesquisa e Planejamento – definição de requisitos e arquitetura', options: { breakLine: true } },
    { text: '2. Desenvolvimento das Bibliotecas (C e Dart)', options: { breakLine: true } },
    { text: '3. Implementação dos Nós (ESP32 LoRa V3 + cartão SD)', options: { breakLine: true } },
    { text: '4. Testes de Comunicação LoRa entre nós', options: { breakLine: true } },
    { text: '5. Integração com IPFS (local e AWS)', options: { breakLine: true } },
    { text: '6. Desenvolvimento da Interface do Usuário', options: { breakLine: true } },
    { text: '7. Testes de Mensagens Pequenas e Arquivos Grandes', options: { breakLine: true } },
    { text: '8. Validação, Documentação e Ajustes Finais', options: { breakLine: true } },
    { text: '9. Apresentação Final do Projeto', options: { breakLine: true } }
], { x: 0.6, y: 0.6, fontSize: 18, color: '000000' });

// Slide 16: Referências (exemplo)
slide = pres.addSlide();
slide.addText('Referências Adicionais', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: '– LoRaWAN Specification v1.0.4', options: { breakLine: true } },
    { text: '– IPFS Docs: https://docs.ipfs.io/', options: { breakLine: true } },
    { text: '– AWS IoT Core – https://aws.amazon.com/iot-core/', options: { breakLine: true } },
    { text: '– ESP32 Datasheet – https://www.espressif.com/en/products/socs/esp32', options: { breakLine: true } },
    { text: '– Biblioteca LoRa Arduino – https://github.com/sandeepmistry/arduino-LoRa', options: { breakLine: true } }
], { x: 0.6, y: 0.6, fontSize: 18, color: '000000' });

// Slide 17: Conclusão
slide = pres.addSlide();
slide.addText('Conclusão e Próximos Passos', { x: 0.5, y: 0.2, fontSize: 28, color: '006400', bold: true });
slide.addText([
    { text: 'Proposta viável de rede descentralizada usando LoRa e IPFS', options: { breakLine: true } },
    { text: '– Escalável: adicionar nós aumenta cobertura e redundância', options: { breakLine: true } },
    { text: '– Segura: criptografia AES‑256 (via bibliotecas) e autentificação', options: { breakLine: true } },
    { text: '– Anônima: identidades apenas por IDs, sem dados pessoais', options: { breakLine: true } },
    { text: '', options: { breakLine: true } },
    { text: 'Próximos Passos:', options: { breakLine: true } },
    { text: '– Implementar gateways LoRaWAN para conexão com internet', options: { breakLine: true } },
    { text: '– Testar em ambiente rural/urbano com nós reais', options: { breakLine: true } },
    { text: '– Aprimorar interface do usuário (usabilidade)', options: { breakLine: true } },
    { text: '– Explorar outras aplicações: OTA, sensoragem, mensagens de voz', options: { breakLine: true } }
], { x: 0.6, y: 0.6, fontSize: 18, color: '000000' });

// Write file
pres.writeFile({ fileName: 'Projeto_IoT2_APresentacao.pptx' });