# Vulkan Wrapper Android — teste de interceptação

Projeto Android Studio (Java + C++17 / CMake / NDK) para ARM64.

1. Abra a pasta no Android Studio.
2. Instale Android SDK 35, NDK 27.0.12077973, CMake e JDK 17.
3. Aguarde o Gradle Sync; conecte seu tablet e execute `app`.
4. Toque **Testar Vulkan**. Deve mostrar GPU e versão Vulkan.
5. Logcat: filtro `VulkanWrapper` ou `adb logcat -s VulkanWrapper:I`.

Este é um **teste de interceptação interna**: não exporta uma camada Vulkan nem funciona como driver do Winlator. Não implementa BC7. A biblioteca chama o loader Vulkan do Android via dlopen e encaminha vkCreateInstance.

Não há gradle wrapper no projeto; Android Studio pode abrir/sincronizar usando Gradle instalado ou criar o wrapper.

## Decodificação BC7 (CPU)

O botão **Testar BC7 CPU** executa um teste de decodificação de um bloco BC7 de 16 bytes para 4x4 RGBA8 (64 bytes). `bc7_decoder.cpp` também disponibiliza `bc7::decodeRGBA8` para imagens BC7 com dimensões não múltiplas de quatro, com validação do tamanho de entrada e do row pitch.

O decodificador utiliza [bcdec](https://github.com/iOrange/bcdec) v0.985 (licença incluída no header). **Não há suporte transparente a BC7 no Vulkan/DXVK:** ainda é necessário integrar upload, alocação e views com imagens RGBA8 e interceptar funções Vulkan com dispatch correto. A GPU não passa a anunciar BC7 por causa deste código.

## Teste de upload BC7 para GPU Vulkan

O botão **Testar BC7 GPU** cria um bloco BC7 de teste, decodifica para RGBA8 com bcdec, cria uma imagem `VK_FORMAT_R8G8B8A8_UNORM`, usa staging buffer com memória host-coherent, executa `vkCmdCopyBufferToImage` e transiciona a imagem para `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.

O teste é limitado a uma imagem 4x4 e não faz leitura de volta da GPU ou amostragem por shader. Não intercepta automaticamente `vkCreateImage` do DXVK, não virtualiza `VkImage` nem cobre mipmaps, imagens em arrays, cópias, atualizações parciais ou formatos sRGB. Não altera os recursos anunciados pelo driver Samsung.

## Visualizador DDS BC7

Use **Abrir textura BC7 (.dds)** para selecionar um DDS com cabecalho DX10, formato DXGI_FORMAT_BC7_UNORM (98) ou BC7_UNORM_SRGB (99), textura 2D e arraySize=1. O primeiro mip e decodificado por bcdec para RGBA8 e exibido por Android Bitmap/ImageView. Tamanho maximo do arquivo: 64 MiB; dimensoes ate 8192x8192. Outros DDS retornam mensagem de formato nao suportado.

**Esta visualizacao nao utiliza um pipeline grafico Vulkan**: serve para validar a imagem BC7 convertida. O teste separado **Testar BC7 GPU** continua exercitando upload Vulkan sem apresentar pixels na tela. Suporte a renderizacao Vulkan com swapchain, mips, arrays e integracao DXVK ainda pendente.
