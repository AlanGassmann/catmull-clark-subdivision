#include <algorithm>
#include <stdexcept>
#include <vector>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Model.hpp"
#include "Window.hpp"
#include "Callbacks.hpp"
#include "Debug.hpp"
#include "Shaders.hpp"
#include "SubDivMesh.hpp"
#include "SubDivMeshRenderer.hpp"
#define VERSION 20251006


// models and settings
std::vector<std::string> models_names = { "cubo", "icosahedron", "plano", "suzanne", "star" };
int current_model = 0;
bool fill = true, nodes = true, wireframe = true, smooth = false, 
	reload_mesh = true, mesh_modified = false;

// extraa callbacks
void keyboardCallback(GLFWwindow* glfw_win, int key, int scancode, int action, int mods);

SubDivMesh mesh;
void subdivide(SubDivMesh &mesh);

int main() {
	
	// initialize window and setup callbacks
	Window window(800, 600, "CG Demo");
	setCommonCallbacks(window);
	glfwSetKeyCallback(window, keyboardCallback);
	CameraSettings &cs = window.getCamera();
	cs.view_fov = 60.f;
	
	// setup OpenGL state and load shaders
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); 
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.8f,0.8f,0.9f,1.f);
	Shader shader_flat("shaders/flat"),
		shader_smooth("shaders/smooth"),
		shader_wireframe("shaders/wireframe");
	SubDivMeshRenderer renderer;
	
	// main loop
	Material material;
	material.ka = material.kd = glm::vec3{.8f,.4f,.4f};
	material.ks = glm::vec3{.5f,.5f,.5f};
	material.shininess = 50.f;
	
	FrameTimer timer;
	do {
		
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
		if (reload_mesh) {
			mesh = SubDivMesh("models/"+models_names[current_model]+".dat");
			reload_mesh = false; mesh_modified = true;
		}
		if (mesh_modified) {
			renderer = makeRenderer(mesh,false);
			mesh_modified = false;
		}
		
		if (nodes) {
			shader_wireframe.use();
			setMatrixes(window, shader_wireframe);
			renderer.drawPoints(shader_wireframe);
		}
		
		if (wireframe) {
			shader_wireframe.use();
			setMatrixes(window, shader_wireframe);
			renderer.drawLines(shader_wireframe);
		}
		
		if (fill) {
			Shader &shader = smooth ? shader_smooth : shader_flat;
			shader.use();
			setMatrixes(window, shader);
			shader.setLight(glm::vec4{2.f,1.f,5.f,0.f}, glm::vec3{1.f,1.f,1.f}, 0.25f);
			shader.setMaterial(material);
			renderer.drawTriangles(shader);
		}
		
		// settings sub-window
		window.ImGuiDialog("CG Example",[&](){
			if (ImGui::Combo(".dat (O)", &current_model,models_names)) reload_mesh = true;
			ImGui::Checkbox("Fill (F)",&fill);
			ImGui::Checkbox("Wireframe (W)",&wireframe);
			ImGui::Checkbox("Nodes (N)",&nodes);
			ImGui::Checkbox("Smooth Shading (S)",&smooth);
			if (ImGui::Button("Subdivide (D)")) { subdivide(mesh); mesh_modified = true; }
			if (ImGui::Button("Reset (R)")) reload_mesh = true;
			ImGui::Text("Nodes: %i, Elements: %i",mesh.n.size(),mesh.e.size());
		});
		
		// finish frame
		window.finishFrame();
		
	} while( glfwGetKey(window,GLFW_KEY_ESCAPE)!=GLFW_PRESS && !glfwWindowShouldClose(window) );
}

void keyboardCallback(GLFWwindow* glfw_win, int key, int scancode, int action, int mods) {
	if (action==GLFW_PRESS) {
		switch (key) {
		case 'D': subdivide(mesh); mesh_modified = true; break;
		case 'F': fill = !fill; break;
		case 'N': nodes = !nodes; break;
		case 'W': wireframe = !wireframe; break;
		case 'S': smooth = !smooth; break;
		case 'R': reload_mesh=true; break;
		case 'O': case 'M': current_model = (current_model+1)%models_names.size(); reload_mesh = true; break;
		}
	}
}

// La struct Arista guarda los dos indices de nodos de una arista
// Siempre pone primero el menor indice, para facilitar la búsqueda en lista ordenada;
//    es para usar con el Mapa de más abajo, para asociar un nodo nuevo a una arista vieja
struct Arista {
	int n[2];
	Arista(int n1, int n2) {
		n[0]=n1; n[1]=n2;
		if (n[0]>n[1]) std::swap(n[0],n[1]);
	}
	Arista(Elemento &e, int i) { // i-esima arista de un elemento
		n[0]=e[i]; n[1]=e[i+1];
		if (n[0]>n[1]) std::swap(n[0],n[1]); // pierde el orden del elemento
	}
	const bool operator<(const Arista &a) const {
		return (n[0]<a.n[0]||(n[0]==a.n[0]&&n[1]<a.n[1]));
	}
};

// Mapa sirve para guardar una asociación entre una arista y un indice de nodo (que no es de la arista)
using Mapa = std::map<Arista,int>;

int agregarCentroide(SubDivMesh &mesh){
	Nodo centroide(glm::vec3(0,0,0)); 			// Centroide a agregar
	int posPrimerCentroide = mesh.n.size(); 	// Posicion del primer centroide
	for (unsigned int i = 0; i< mesh.e.size(); i++){
		centroide.p = glm::vec3(0,0,0);			
		for (unsigned int j = 0; j < mesh.e[i].nv; j++)
			centroide.p += mesh.n[mesh.e[i].n[j]].p;
		
		centroide.p /= static_cast<float>(mesh.e[i].nv);
		mesh.n.push_back(centroide);
	}
	return posPrimerCentroide;
}
void agregarPAristas(SubDivMesh &mesh, int posPrimerCentroide, Mapa &mapaAristas){

	Nodo nodoMedio(glm::vec3(0,0,0));  	//Nodo a agregar
	for (unsigned int i = 0; i< mesh.e.size(); i++){		
		for (unsigned int j = 0; j< mesh.e[i].nv; j++){
			//Calculo los indices de los nodos que conforman la arista.
			int indiceNodo1 = mesh.e[i][j];
			int indiceNodo2 = mesh.e[i][j+1];
			
			//Verifico si la arista ya fue procesada por otra cara.
			Arista arista(indiceNodo1, indiceNodo2);
			if (mapaAristas.find(arista) != mapaAristas.end()) {
				continue;}
			
			//Calculo la posicion del nodo a agregar, teniendo en cuenta si es frontera.
			if (mesh.e[i].v[j] == -1){
				nodoMedio.p = (mesh.n[indiceNodo1].p 
							   + mesh.n[indiceNodo2].p) 
					/ 2.0f;
			}else{
				nodoMedio.p = (mesh.n[indiceNodo1].p 
					   + mesh.n[indiceNodo2].p 
					   + mesh.n[posPrimerCentroide + i].p
					   + mesh.n[posPrimerCentroide + mesh.e[i].v[j]].p) 
					/ 4.0f;
			}
			mapaAristas[arista] = mesh.n.size();
			mesh.n.push_back(nodoMedio);
		}
	}
}
void armarElementos(SubDivMesh &mesh, int posPrimerCentroide, Mapa &mapaAristas ){
	unsigned int primerNuevoElem = mesh.e.size();
	for (unsigned int i =0; i< primerNuevoElem ; i++){
		// crear los nuevos elemento 
		int centroide = posPrimerCentroide + i;
		for (unsigned int j = 1; j < mesh.e[i].nv; j++){
			int indiceNodo1 = mesh.e[i][j];
			int indiceNodo2 = mesh.e[i][j+1];
			int indiceNodo3 = mesh.e[i][j+2];
			
			Arista a1(indiceNodo1,indiceNodo2);
			Arista a2(indiceNodo2,indiceNodo3);
			mesh.agregarElemento(centroide, mapaAristas[a1] , indiceNodo2, mapaAristas[a2]);

		}
		// Sustituir el primer elemento
		int indiceNodo1 = mesh.e[i][0];
		int indiceNodo2 = mesh.e[i][1];
		int indiceNodo3 = mesh.e[i][2];
		
		Arista a1(indiceNodo1,indiceNodo2);
		Arista a2(indiceNodo2,indiceNodo3);
		mesh.reemplazarElemento(i, centroide, mapaAristas[a1], indiceNodo2, mapaAristas[a2]);
		
	}
	
}
	
void calcularNuevasPos(SubDivMesh &mesh, int posPrimerCentroide){
	std::vector<glm::vec3> nuevaPosNodos(posPrimerCentroide);
		
	for(unsigned int i = 0; i < posPrimerCentroide; i++){
		int n = mesh.n[i].e.size();
		glm::vec3 p = mesh.n[i].p;
			
		// Promedio de centroides (F)
		glm::vec3 f(0,0,0);
		for (unsigned int j = 0; j < n; j++){
			int id_cara = mesh.n[i].e[j];
			int id_centroide = mesh.e[id_cara].n[0]; // Centroide en la pos 0[cite: 2]
			f += mesh.n[id_centroide].p;
		}
		f = f / static_cast<float>(n);
		
		// Promedio de puntos de aristas (R)
		glm::vec3 r(0,0,0);
		int count_r = 0;
			
		for (unsigned int j = 0; j < n; j++){
			int id_cara = mesh.n[i].e[j];
			int arista1_idx = mesh.e[id_cara].n[1]; // Arista 1 en pos 1
			int arista2_idx = mesh.e[id_cara].n[3]; // Arista 2 en pos 3
				
			if (mesh.n[i].es_frontera) {
				// Si el nodo es frontera, solo sumamos las aristas que también son frontera
				if (mesh.n[arista1_idx].es_frontera) { r += mesh.n[arista1_idx].p; count_r++; }
				if (mesh.n[arista2_idx].es_frontera) { r += mesh.n[arista2_idx].p; count_r++; }
			} else {
				r += mesh.n[arista1_idx].p + mesh.n[arista2_idx].p;
				count_r += 2;}
		}
		
		if (count_r > 0) 
			r = r / static_cast<float>(count_r);
			
		// Aplicar fórmula
		if(mesh.n[i].es_frontera){	
			nuevaPosNodos[i] = (r + p) / 2.0f;
		} else {				
			nuevaPosNodos[i] = (4.0f * r - f + static_cast<float>(n - 3) * p) / static_cast<float>(n);
		}
	}
		
		// Actualizar nuevas posiciones
		for(unsigned int i = 0; i < posPrimerCentroide; i++){
			mesh.n[i].p = nuevaPosNodos[i];
		}
}
void subdivide(SubDivMesh &mesh) {
	mesh.n.reserve(mesh.n.size() + mesh.e.size() * 3);
	/// @@@@@: Implementar Catmull-Clark... lineamientos:
	//  Los nodos originales estan en las posiciones 0 a #n-1 de m.n,
	//  Los elementos orignales estan en las posiciones 0 a #e-1 de m.e
	//  1) Por cada elemento, agregar el centroide (nuevos nodos: #n a #n+#e-1)
	int posPrimerCentroide = agregarCentroide(mesh);
	//  2) Por cada arista de cada cara, agregar un pto en el medio que es
	//      promedio de los vertices de la arista y los centroides de las caras 
	//      adyacentes. Aca hay que usar los elementos vecinos.
	//      En los bordes, cuando no hay vecinos, es simplemente el promedio de los 
	//      vertices de la arista
	//      Hay que evitar procesar dos veces la misma arista (como?)
	//      Mas adelante vamos a necesitar determinar cual punto agregamos en cada
	//      arista, y ya que no se pueden relacionar los indices con una formula simple
	//      se sugiere usar Mapa como estructura auxiliar
	Mapa mapaAristas;
	agregarPAristas(mesh, posPrimerCentroide, mapaAristas);
	//  3) Armar los elementos nuevos
	//      Los quads se dividen en 4, (uno reemplaza al original, los otros 3 se agregan)
	//      Los triangulos se dividen en 3, (uno reemplaza al original, los otros 2 se agregan)
	//      Para encontrar los nodos de las aristas usar el mapa que armaron en el paso 2
	//      Ordenar los nodos de todos los elementos nuevos con un mismo criterio (por ej, 
	//      siempre poner primero al centroide del elemento), para simplificar el paso 4.
	armarElementos(mesh, posPrimerCentroide, mapaAristas);
	mesh.makeVecinos();
	//  4) Calcular las nuevas posiciones de los nodos originales
	//      Para nodos interiores: (4r-f+(n-3)p)/n
	//         f=promedio de nodos interiores de las caras (los agregados en el paso 1)
	//         r=promedio de los pts medios de las aristas (los agregados en el paso 2)
	//         p=posicion del nodo original
	//         n=cantidad de elementos para ese nodo
	//      Para nodos del borde: (r+p)/2
	//         r=promedio de los dos pts medios de las aristas
	//         p=posicion del nodo original
	//      Ojo: en el paso 3 cambio toda la SubDivMesh, analizar donde quedan en los nuevos 
	//      elementos (¿de que tipo son?) los nodos de las caras y los de las aristas 
	//      que se agregaron antes.
	calcularNuevasPos(mesh, posPrimerCentroide);
	// tips:
	//   no es necesario cambiar ni agregar nada fuera de este método, (con Mapa como 
	//     estructura auxiliar alcanza)
	//   sugerencia: probar primero usando el cubo (es cerrado y solo tiene quads)
	//               despues usando la piramide (tambien cerrada, y solo triangulos)
	//               despues el ejemplo plano (para ver que pasa en los bordes)
	//               finalmente el mono (tiene mezcla y elementos sin vecinos)
	//   repaso de como usar un mapa:
	//     para asociar un indice (i) de nodo a una arista (n1-n2): elmapa[Arista(n1,n2)]=i;
	//     para saber si hay un indice asociado a una arista:  ¿elmapa.find(Arista(n1,n2))!=elmapa.end()?
	//     para recuperar el indice (en j) asociado a una arista: int j=elmapa[Arista(n1,n2)];
	
	
	// Esta llamada valida si la estructura de datos quedó consistente (si todos los
	// índices están dentro del rango válido, y si son correctas las relaciones
	// entre los .n de los elementos y los .e de los nodos). Mantener al final de
	// esta función para ver que la subdivisión implementada no rompa esos invariantes.

	mesh.verificarIntegridad();
}
