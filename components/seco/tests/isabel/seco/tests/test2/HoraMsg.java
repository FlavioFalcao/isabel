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
package isabel.seco.tests.test2;

import isabel.seco.network.javaser.*;

import java.sql.Time;
import java.util.Calendar;
import java.util.Date;

public class HoraMsg implements JavaMessage {
	public int hora;

	public int minuto;

	public int segundo;

	Calendar myCalendar = Calendar.getInstance();

	public HoraMsg() {
		hora = myCalendar.get(Calendar.HOUR);
		minuto = myCalendar.get(Calendar.MINUTE);
		segundo = myCalendar.get(Calendar.SECOND);

	}
	// long d = myCalendar.getTimeInMillis();
	// String hora = myCalendar.toString();

}
