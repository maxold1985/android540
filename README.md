# Vulkan Wrapper Android — teste de interceptação

Projeto Android Studio (Java + C++17 / CMake / NDK) para ARM64.

1. Abra a pasta no Android Studio.
2. Instale Android SDK 35, NDK 27.0.12077973, CMake e JDK 17.
3. Aguarde o Gradle Sync; conecte seu tablet e execute `app`.
4. Toque **Testar Vulkan**. Deve mostrar GPU e versão Vulkan.
5. Logcat: filtro `VulkanWrapper` ou `adb logcat -s VulkanWrapper:I`.

Este é um **teste de interceptação interna**: não exporta uma camada Vulkan nem funciona como driver do Winlator. Não implementa BC7. A biblioteca chama o loader Vulkan do Android via dlopen e encaminha vkCreateInstance.

Não há gradle wrapper no projeto; Android Studio pode abrir/sincronizar usando Gradle instalado ou criar o wrapper.
