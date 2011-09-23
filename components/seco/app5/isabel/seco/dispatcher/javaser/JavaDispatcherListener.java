/*
 * ISABEL: A group collaboration tool for the Internet
 * Copyright (C) 2009 Agora System S.A.
 * 
 * This file is part of Isabel.
 * 
 * Isabel is free software: you can redistribute it and/or modify
 * it under the terms of the Affero GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Isabel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Affero GNU General Public License for more details.
 * 
 * You should have received a copy of the Affero GNU General Public License
 * along with Isabel.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * JavaDispatcherListener.java
 *
 */

package isabel.seco.dispatcher.javaser;

import isabel.seco.network.javaser.JavaMessage;

/**
 *  Interfaz que implementan los objetos capaces de procesar mensajes
 *  de tipo JavaMessage.
 *
 *  @author  Santiago Pavon
 *  @author  Fernando Escribano
 *  @version 1.0
 */
public interface JavaDispatcherListener {
    
    /**
     * Procesa un mensaje.
     * @param msg Mensaje a procesar.
     */
    public void processMessage(JavaMessage msg);
    
}

