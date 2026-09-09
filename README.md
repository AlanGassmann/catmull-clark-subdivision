# Proyecto: Subdivision Surfaces (Catmull-Clark)

Este repositorio contiene la implementación en C++ del algoritmo de subdivisión de superficies de Catmull-Clark, desarrollado para el Trabajo Práctico de Computación Gráfica. La aplicación utiliza OpenGL, GLFW y GLAD para renderizar modelos 3D y observar el efecto del suavizado geométrico de forma interactiva.

## Créditos y Autoría
* **Código base y consigna original**: Prof. Pablo Novara (FICH-UNL).

## Características del Proyecto
* **Modelos soportados**: El sistema permite cargar y procesar mallas de prueba iniciales como el cubo, icosaedro, plano, suzanne y estrella.
* **Topología mixta**: El algoritmo base soporta tanto triángulos como cuadriláteros en la malla original.
* **Visualización en tiempo real**: Incluye opciones para alternar la vista entre polígonos con relleno, malla de alambre, visualización de nodos y sombreado plano o suave.

## Estructuras de Datos
La representación topológica se gestiona mediante la clase `SubDivMesh`, que almacena de forma estructurada los componentes de la geometría:
* **Elemento**: Estructura que representa polígonos (triángulos o quads), guardando los índices de sus respectivos vértices y los índices de sus caras vecinas que comparten aristas.
* **Nodo**: Estructura para los vértices que almacena las coordenadas espaciales (`glm::vec3`), la lista de caras incidentes y un valor booleano (`es_frontera`) para identificar límites abiertos en la geometría.
* **Mapa de Aristas**: Se emplea la estructura `std::map<Arista, int>` para asociar de manera unívoca los nuevos vértices intermedios con sus respectivas aristas, garantizando que no se procesen de forma duplicada.

## Implementación del Algoritmo
El proceso matemático de subdivisión se estructuró en cuatro etapas secuenciales, cumpliendo con las normativas teóricas de Catmull-Clark:
1. **Generación de Centroides**: Por cada elemento de la malla original, se calculó e inyectó un nuevo nodo espacial ubicado en el promedio exacto de sus vértices.
2. **Puntos de Arista**: Se insertaron nodos en el centro topológico de las aristas. Para el interior de la malla, la posición se obtuvo promediando los vértices de la arista y los centroides de las dos caras que la comparten. En las aristas de frontera, el nodo se calculó únicamente como el promedio de los dos vértices.
3. **Reconstrucción Topológica**: Cada elemento original se subdividió en múltiples cuadriláteros (3 en triángulos, 4 en quads). Cada nuevo quad se formó conectando el centroide de la cara, dos nodos de arista contiguos y el vértice original compartido. Un quad nuevo reemplazó al elemento original en memoria, mientras los restantes fueron añadidos al final del arreglo. Al finalizar, se ejecutó `makeVecinos` para recalcular las incidencias de los polígonos.
4. **Actualización de Posiciones Originales**: Se desplazaron las coordenadas de los nodos de la malla inicial para asegurar el suavizado de la superficie. Los nodos de frontera se ubicaron utilizando la fórmula $(r+p)/2$, donde $r$ es el promedio de las aristas adyacentes y $p$ la posición original. Para nodos internos, se implementó la ecuación ponderada $(4r-f+(n-3)p)/n$, utilizando el promedio de centroides $f$ y la valencia del vértice $n$.

## Aspectos Técnicos Destacados
* **Gestión de Memoria (Hardware Optimization)**: Implementación de pre-reserva de memoria (`reserve()`) en el vector de nodos antes de subdividir. Esto previene los cuellos de botella de la CPU ocasionados por el realojamiento dinámico de memoria RAM al escalar exponencialmente la cantidad de vértices.
* **Seguridad de Estado (State Safety)**: Protección de los datos en la reconstrucción topológica. Se postergó la sobrescritura del elemento original hasta que todos los nuevos sub-elementos fueron procesados e insertados, erradicando los riesgos de mutación de estado concurrente.
* **Seguridad de Tipos (Type Safety)**: Estandarización de bucles de iteración con tipos `unsigned int` para alinear con el retorno de los contenedores de la STL, asegurando un código limpio de advertencias bajo compiladores estrictos.

## Controles de la Aplicación
* **D**: Ejecutar algoritmo de subdivisión.
* **R**: Reiniciar la malla a su topología inicial.
* **O / M**: Navegar entre los diferentes modelos 3D disponibles.
* **F / W / N / S**: Conmutar las vistas de Fill, Wireframe, Nodes y Smooth.
* **ESC**: Finalizar y cerrar la ventana.
