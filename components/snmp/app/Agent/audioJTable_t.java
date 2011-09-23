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

import javax.swing.table.*;

class audioJTable_t extends ISJTable {

    audioDataBase_t audioDataBase;
    MonitorGUI_t MGUI;

    audioJTable_t (TableModel tm,
                   MonitorGUI_t MGUI,
                   String cId,
                   audioDataBase_t audDB
                  ) {
        super (tm, cId);
        audioDataBase = audDB;
        this.MGUI = MGUI;
    }

    sck.Oid getOidForGraph () {
        int dataIndex = audioDataBase.getChIndexFromPos(selectedRow);
        try {
            sck.Oid oid = new sck.Oid (DataBase_t.AUDIO+".1.1."+(selectedCol+1)+"."+dataIndex);
            return oid;
        } catch (Exception exc) {
            System.out.println("Excepcion capturada:"+exc);
        };
        return null;
    }

    String getTitleForGraph() {
        int dataIndex = audioDataBase.getChIndexFromPos(selectedRow);
        return selectedCompId+" | "+" Canal: "+dataIndex+" -->"+dataModel.getColumnName(selectedCol);
    }

    void putSelectedTable () {
        MGUI.setSelectedTable("AUD");
    };
}

