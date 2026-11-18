package org.bitskim.android;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.*;
import java.util.*;

/** Android grants read access to this selected document tree through the picker. */
final class ProviderFolder implements TransferFiles.Source {
    private final ContentResolver resolver;
    private final Uri tree;
    private static final String[] COLUMNS={DocumentsContract.Document.COLUMN_DOCUMENT_ID,DocumentsContract.Document.COLUMN_DISPLAY_NAME,DocumentsContract.Document.COLUMN_MIME_TYPE,DocumentsContract.Document.COLUMN_SIZE,DocumentsContract.Document.COLUMN_FLAGS};
    ProviderFolder(ContentResolver resolver,Uri tree)throws IOException {if(tree==null||!"content".equals(tree.getScheme())||!DocumentsContract.isTreeUri(tree))throw new IOException("Choose a folder through Android Files.");this.resolver=resolver;this.tree=tree;}
    TransferFiles.Node root()throws IOException {
        String id=DocumentsContract.getTreeDocumentId(tree);try(Cursor cursor=resolver.query(uri(id),COLUMNS,null,null,null)){
            if(cursor==null||!cursor.moveToFirst())throw new IOException("The provider could not open the selected folder.");TransferFiles.Node node=read(cursor);if(!node.directory)throw new IOException("Choose a folder.");return node;
        }catch(SecurityException e){throw new IOException("Folder access was denied. Choose it again in Android Files.",e);}
    }
    private Uri uri(String id){return DocumentsContract.buildDocumentUriUsingTree(tree,id);}
    private TransferFiles.Node read(Cursor cursor)throws IOException {
        String id=cursor.getString(0),name=cursor.getString(1),mime=cursor.getString(2);if(id==null||name==null)throw new IOException("The provider returned an unnamed item.");
        if(!cursor.isNull(4)&&(cursor.getLong(4)&DocumentsContract.Document.FLAG_VIRTUAL_DOCUMENT)!=0)throw new IOException("This provider exposes a virtual document. Download an ordinary copy first.");
        return new TransferFiles.Node(id,name,DocumentsContract.Document.MIME_TYPE_DIR.equals(mime),cursor.isNull(3)?-1:cursor.getLong(3));
    }
    public List<TransferFiles.Node> children(TransferFiles.Node parent)throws IOException {
        List<TransferFiles.Node> children=new ArrayList<>();Uri query=DocumentsContract.buildChildDocumentsUriUsingTree(tree,parent.id);
        try(Cursor cursor=resolver.query(query,COLUMNS,null,null,null)){
            if(cursor==null)throw new IOException("The provider could not list this folder.");while(cursor.moveToNext()){if(children.size()>=TransferFiles.MAX_ENTRIES)throw new IOException("The folder has too many entries.");children.add(read(cursor));}
        }catch(SecurityException e){throw new IOException("Folder permission changed. Choose it again.",e);}return children;
    }
    public InputStream open(TransferFiles.Node file)throws IOException {try{return resolver.openInputStream(uri(file.id));}catch(SecurityException e){throw new IOException("The provider denied access to a file. Choose the folder again.",e);}}
}
